use std::thread;
use std::time::{Duration, Instant};

use crate::dev;
use crate::types::{Error, Result, Device, Interrupts};

use log::info;

use pc_hal::prelude::*;
use pc_hal_util::pci::map_bar;

impl<E, IM, PD, D, Res> Device<E, IM, PD, D, Res>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>
{
    
    pub fn init<B, MM, Dma, ICU>(
        vbus: &mut B,
        num_rx_queues: u16,
        num_tx_queues: u16,
        interrupt_timeout: i16,
    ) -> Result<Device<E, IM, PD, D, Res>, E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
        MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
        Dma: pc_hal::traits::DmaSpace,
        ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    {
        // TODO: this should be reworked to support actual device discovery
        let mut devices = vbus.device_iter();
        let l40009 = devices.next().unwrap();
        let icu = ICU::from_device(&vbus, l40009)?;
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

        let mut dev = Device {
            bar0,
            num_rx_queues,
            num_tx_queues,
            rx_queues,
            tx_queues,
            device: nic,
            interrupts: Interrupts {
                enabled: false
            }
        };

        // TODO some vfio based interrupt setup happens here. but it is optinal, investigate

        dev.reset_and_init()?;

        Ok(dev)
    }

    fn reset_and_init(&mut self) -> Result<(), E> {
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

        Ok(())
    }

    fn reset_stats(&mut self) {
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
