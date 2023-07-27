use std::{thread, mem};
use std::time::{Duration, Instant};

use crate::constants::{NUM_RX_QUEUE_ENTRIES, NUM_TX_QUEUE_ENTRIES, PKT_BUF_ENTRY_SIZE};
use crate::dev;
use crate::dma::{DmaMemory, Mempool};
use crate::types::{Error, Result, Device, Interrupts, ixgbe_adv_rx_desc, RxQueue};

use log::{info, trace};

use pc_hal::prelude::*;
use pc_hal_util::pci::map_bar;

impl<E, IM, PD, D, Res, Dma, MM> Device<E, IM, PD, D, Res, Dma, MM>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    
    pub fn init<B, ICU>(
        bus: &mut B,
        num_rx_queues: u16,
        num_tx_queues: u16,
        interrupt_timeout: i16,
    ) -> Result<Device<E, IM, PD, D, Res, Dma, MM>, E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
        ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    {
        // TODO: this should be reworked to support actual device discovery
        let mut devices = bus.device_iter();
        let l40009 = devices.next().unwrap();
        let icu = ICU::from_device(&bus, l40009)?;
        let _pci_bridge = devices.next().unwrap();
        let mut nic = PD::try_of_device(devices.next().unwrap()).unwrap();
        info!("Obtained handles to ICU and NIC");

        let vendor_id = nic.read16(0x0)?;
        let device_id = nic.read16(0x2)?;
        info!("NIC has vendor ID: 0x{:x}, device ID: 0x{:x}", vendor_id, device_id);

        if vendor_id != dev::VENDOR_ID || device_id != dev::DEVICE_ID {
            return Err(Error::IncompatibleDevice);
        }

        if !(num_rx_queues > 0 && num_rx_queues <= dev::MAX_QUEUES) || !(num_tx_queues > 0 && num_tx_queues <= dev::MAX_QUEUES) {
            return Err(Error::InvalidQueueCount);
        }

        let bar0_mem : IM = map_bar(&mut nic, 0)?;
        let bar0 = dev::Intel82559ES::Bar0::new(bar0_mem);

        let rx_queues = Vec::with_capacity(num_rx_queues as usize);
        let tx_queues = Vec::with_capacity(num_tx_queues as usize);

        trace!("Assigning DMA Domain");
        let mut dma_space = Dma::new();
        let mut dma_domain = nic.resource_iter().filter(|r| r.typ() == ResourceType::DmaDomain).next().unwrap();
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space)?;

        let mut dev = Device {
            bar0,
            num_rx_queues,
            num_tx_queues,
            rx_queues,
            tx_queues,
            device: nic,
            interrupts: Interrupts {
                enabled: false
            },
            dma_space
        };

        // TODO some vfio based interrupt setup happens here in ixy.
        // it is optional in ixy because there used to be a time when it was apparently a poll mode driver
        // we definitely want interrupts here

        dev.reset_and_init::<B>(bus)?;

        Ok(dev)
    }

    fn reset_and_init<B>(&mut self, bus: &mut B) -> Result<(), E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    {
        info!("Resetting device");
        // section 4.6.3.1 - disable all interrupts
        self.disable_interrupts();

        // Do a link and software reset
        self.bar0.ctrl().modify(|_, w|
            w.lrst(1).rst(1)
        );
        
        // Wait until the reset is done
        loop {
            let ctrl = self.bar0.ctrl().read();
            if ctrl.lrst() == 0 && ctrl.rst() == 0 {
                break;
            }
        }
        // Just to be sure wait the length that the datasheet tells us to
        thread::sleep(Duration::from_millis(10));

        // section 4.6.3.1 - disable interrupts again after reset
        self.disable_interrupts();

        // TODO: I believe strictly speaking we need to wait until the EEPROM read is done for this
        let mac = self.get_mac_addr();
        info!("initializing device");
        info!(
            "mac address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        );

        // section 4.6.3 - wait for EEPROM auto read completion
        loop {
            let eec = self.bar0.eec().read();
            if eec.auto_rd() == 1 {
                break;
            }
        }

        // section 4.6.3 - wait for dma initialization done
        loop {
            let rdrxctl = self.bar0.rdrxctl().read();
            if rdrxctl.dmaidone() == 1 {
                break;
            }
        }

        // section 4.6.4 - initialize link (auto negotiation)
        self.init_link();

        // unlike ixy we temporarly wait for the link right here to check that it works
        self.wait_for_link()?;

        // section 4.6.5 - statistical counters
        // reset-on-read registers, just read them once
        self.reset_stats();

         // section 4.6.7 - init rx
        self.init_rx(bus)?;

        // section 4.6.8 - init tx
        self.init_tx(bus)?;

        Ok(())
    }

    fn init_tx<B>(&mut self, bus: &mut B) -> Result<(), E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    {
    }

    fn init_rx<B>(&mut self, bus: &mut B) -> Result<(), E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    {
        info!("Initializing RX");

        // disable rx while re-configuring it
        self.bar0.rxctrl().modify(|_, w| w.rxen(0));

        // section 4.6.11.3.4 - allocate all queues and traffic to PB0
        // TODO: constant? it's 128 KB
        self.bar0.rxpbsize(0).modify(|_, w| w.size(128));
        for i in 1..8 {
            self.bar0.rxpbsize(i).modify(|_, w| w.size(0));
        }

        // enable CRC offloading
        self.bar0.hlreg0().modify(|_, w| w.rxcrstrip(1));
        self.bar0.rdrxctl().modify(|_, w| w.crcstrip(1));

        // accept broadcast packets
        self.bar0.fctrl().modify(|_, w| w.bam(1));

        // configure queues, same for all queues
        for i in 0..self.num_rx_queues {
            info!("Initializing RX queue {}", i);

            // TODO: constant, this is advanced one buffer
            // enable advanced rx descriptors
            self.bar0.srrctl(i.into()).modify(|_, w| w.desctype(0b001));

            // let nic drop packets if no rx descriptor is available instead of buffering them
            self.bar0.srrctl(i.into()).modify(|_, w| w.drop_en(1));

            // section 7.1.9 - setup descriptor ring
            let ring_size_bytes =
                NUM_RX_QUEUE_ENTRIES as usize * mem::size_of::<ixgbe_adv_rx_desc>();
            
            info!("Allocating {} bytes for RX queue descriptor ring {}", ring_size_bytes, i);

            let mut descriptor_mem: DmaMemory<E, Dma, MM> = DmaMemory::new(ring_size_bytes, &self.dma_space)?;

            // initialize to 0xff to prevent rogue memory accesses on premature dma activation
            for i in 0..ring_size_bytes {
                descriptor_mem.write8(i, 0xff);
            }

            self.bar0.rdbal(i.into()).write(|w| w.rdbal((descriptor_mem.device_addr() & 0xffff_ffff) as u32));
            self.bar0.rdbah(i.into()).write(|w| w.rdbah((descriptor_mem.device_addr() >> 32) as u32));
            self.bar0.rdlen(i.into()).write(|w| w.len(ring_size_bytes as u32));

            // set ring to empty at start
            self.bar0.rdh(i.into()).modify(|_, w| w.rdh(0));
            self.bar0.rdt(i.into()).modify(|_, w| w.rdt(0));

            info!("Set up of RX queue DMA for ring {} at device addr: 0x{:x} done", i, descriptor_mem.device_addr());

            // TODO: ixy does a thing where they limit it to page size here, let's check if we have to do this
            //let mempool_size = if NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES < 4096 {
            //    4096
            //} else {
            //    NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES
            //};
            // I am also confused as to why we have the sum here?
            let mempool_entries = NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES;

            let mempool = Mempool::new(mempool_entries, PKT_BUF_ENTRY_SIZE, &self.dma_space)?;

            let queue = RxQueue {
                descriptors: descriptor_mem,
                pool: mempool,
                num_descriptors: NUM_RX_QUEUE_ENTRIES,
                rx_index: 0,
                bufs_in_use: Vec::with_capacity(NUM_RX_QUEUE_ENTRIES),
            };

            self.rx_queues.push(queue);
            info!("RX queue {} initialized", i);
        }

        // last sentence of section 4.6.7 - set some magic bits
        self.bar0.ctrl_ext().modify(|_, w| w.ns_dis(1));

        // This reserved field says it wants to be set to 0 but is initially 1
        for i in 0..self.num_rx_queues {
            self.bar0.dca_rxctl(i).modify(|_, w| w.reserved3(0));
        }

        // start rx
        self.bar0.rxctrl().modify(|_, w| w.rxen(1));
        
        info!("RX initialization done");

        Ok(())
    }

    fn reset_stats(&mut self) {
        info!("Resetting statistic registers");
        // There are many more counters on the device but we only use these for now
        self.bar0.gprc().read();
        self.bar0.gptc().read();
        self.bar0.gorcl().read();
        self.bar0.gorch().read();
        self.bar0.gotcl().read();
        self.bar0.gotch().read();
    }

    fn init_link(&mut self) {
        info!("Initialising link");
        // We use SFI and XAUI because ixy does as well, if this fails we can probably conclude this is wrong for our card
        // TODO: make constants out of this or support enums in generated API
        self.bar0.autoc().modify(|_, w| w.lms(0b11).teng_pma_pmd_parallel(0b00));
        // Start negotiation
        self.bar0.autoc().modify(|_, w| w.restart_an(1));
    }

    fn get_link_speed(&mut self) -> Option<u16> {
        let links = self.bar0.links().read();
        if links.link_up() == 0 {
            return None;
        }
        // TODO: constants / enums
        match links.link_speed() {
            0b00 => None,
            0b01 => Some(100),
            0b10 => Some(1000),
            0b11 => Some(10000),
            _ => unreachable!()
        }
    }

    fn wait_for_link(&mut self) -> Result<(), E> {
        // TODO: unclear how this will map to kani
        info!("Waiting for link");
        let time = Instant::now();
        while time.elapsed().as_secs() < 10 {
            if let Some(s) = self.get_link_speed() {
                info!("link speed is {} Mbit/s", s);
                return Ok(());
            }
            thread::sleep(Duration::from_millis(100));
        }

        return Err(Error::LinkDown);
    }

    pub fn get_mac_addr(&mut self) -> [u8; 6] {
        let low = self.bar0.ral0().read().ral();
        let high = self.bar0.rah0().read().rah();
        [
            (low & 0xff) as u8,
            (low >> 8 & 0xff) as u8,
            (low >> 16 & 0xff) as u8,
            (low >> 24) as u8,
            (high & 0xff) as u8,
            (high >> 8 & 0xff) as u8,
        ]
    }

    fn clear_interrupts(&mut self) {
        // 31 ones, the last bit in the register is reserved
        self.bar0.eimc().write(|w| w.interrupt_mask(0b1111111111111111111111111111111));
        self.bar0.eicr().read();
    }

    fn disable_interrupts(&mut self) {
        self.bar0.eims().write(|w| w.interrupt_enable(0x0));
        self.clear_interrupts();
    }
}
