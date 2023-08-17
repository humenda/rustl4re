use std::collections::VecDeque;
use std::rc::Rc;
use std::time::{Duration, Instant};
use std::{mem, thread};

use crate::constants::{
    ADV_TX_DESC_DTYP_DATA, AUTOC_LMS_10G_SFI, INTERRUPT_INITIAL_INTERVAL, LINKS_LINK_SPEED_100M,
    LINKS_LINK_SPEED_10G, LINKS_LINK_SPEED_1G, NUM_RX_QUEUE_ENTRIES, NUM_TX_QUEUE_ENTRIES,
    PKT_BUF_ENTRY_SIZE, SRRCTL_DESCTYPE_ADV_ONE_BUFFER,
};
use crate::dev;
use crate::dma::{DmaMemory, Mempool, Packet};
use crate::types::{
    Device, DeviceStats, Error, Interrupts, InterruptsQueue, Result, RxQueue, TxQueue,
};

use log::{info, trace};

use pc_hal::prelude::*;
use pc_hal_util::mmio::MsixDev;
use pc_hal_util::pci::{enable_bus_master, map_bar, map_msix_cap};

impl<E, IM, PD, D, Res, Dma, MM, ISR> Device<E, IM, PD, D, Res, Dma, MM, ISR>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error = E>,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res> + AsMut<D>,
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
    ISR: pc_hal::traits::IrqHandler,
{
    pub fn init<B, ICU>(
        bus: &mut B,
        icu: &mut ICU,
        mut nic: PD,
        num_rx_queues: u8,
        num_tx_queues: u8,
        interrupt_timeout: i16,
    ) -> Result<Device<E, IM, PD, D, Res, Dma, MM, ISR>, E>
    where
        B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
        ICU: pc_hal::traits::Icu<Bus = B, Device = D, Error = E, IrqHandler = ISR>,
    {
        let vendor_id = nic.read16(0x0)?;
        let device_id = nic.read16(0x2)?;
        info!(
            "NIC has vendor ID: 0x{:x}, device ID: 0x{:x}",
            vendor_id, device_id
        );

        if vendor_id != dev::VENDOR_ID || device_id != dev::DEVICE_ID {
            return Err(Error::IncompatibleDevice);
        }

        if !(num_rx_queues > 0 && num_rx_queues <= dev::MAX_QUEUES)
            || !(num_tx_queues > 0 && num_tx_queues <= dev::MAX_QUEUES)
        {
            return Err(Error::InvalidQueueCount);
        }

        let bar0_mem: IM = map_bar(&mut nic, 0)?;
        let bar0 = dev::Intel82559ES::Bar0::new(bar0_mem);

        let msix_mem: IM = map_msix_cap(&mut nic)?.ok_or(Error::MsixMissing)?;
        let msix = MsixDev::Msix::new(msix_mem);

        let rx_queues = Vec::with_capacity(num_rx_queues.into());
        let tx_queues = Vec::with_capacity(num_tx_queues.into());

        trace!("Assigning DMA Domain");
        let mut dma_space = Dma::new();
        let mut dma_domain = nic
            .resource_iter()
            .filter(|r| r.typ() == ResourceType::DmaDomain)
            .next()
            .unwrap();
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space)?;

        let mut dev = Device {
            bar0,
            msix,
            num_rx_queues,
            num_tx_queues,
            rx_queues,
            tx_queues,
            device: nic,
            interrupts: Interrupts {
                timeout_ms: interrupt_timeout,
                // specified in 2us units
                itr_rate: 40,
                queues: Vec::with_capacity(num_rx_queues.into()),
            },
            dma_space,
        };

        //dev.setup_interrupts(icu)?;

        dev.reset_and_init()?;

        Ok(dev)
    }

    fn setup_interrupts<B, ICU>(&mut self, icu: &mut ICU) -> Result<(), E>
    where
        B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
        ICU: pc_hal::traits::Icu<Bus = B, Device = D, Error = E, IrqHandler = ISR>,
        ISR: pc_hal::traits::IrqHandler,
    {
        info!("Setting up MSI-X interrupts");
        assert!(icu.nr_msis()? > 0);
        let nic_dev: &mut D = self.device.as_mut();

        for rx_queue in 0..self.num_rx_queues {
            info!("Setting up MSIX interrupt for queue {}", rx_queue);

            let irq_num = rx_queue;
            let mut irq_handler = icu.bind_msi(irq_num.into(), nic_dev)?;
            self.msix
                .tadd(irq_num.into())
                .modify(|_, w| w.tadd((irq_handler.msi_addr() & 0xffffffff) as u32));
            self.msix
                .tuadd(rx_queue.into())
                .modify(|_, w| w.tuadd((irq_handler.msi_addr() >> 32) as u32));
            self.msix
                .tmsg(rx_queue.into())
                .modify(|_, w| w.tmsg(irq_handler.msi_data()));
            self.msix.tvctrl(rx_queue.into()).modify(|_, w| w.masked(1));

            icu.unmask(&mut irq_handler)?;

            let queue = InterruptsQueue {
                last_time_checked: Instant::now(),
                rx_pkts: 0,
                moving_avg: Default::default(),
                interval: INTERRUPT_INITIAL_INTERVAL,
                instr_counter: 0,
                isr: irq_handler,
            };
            info!("Set up MSIX interrupt for queue {}", rx_queue);
            self.interrupts.queues.push(queue);
        }
        info!("Set up MSI-X interrupts");

        Ok(())
    }

    fn reset_and_init(&mut self) -> Result<(), E> {
        info!("Resetting device");
        // section 4.6.3.1 - disable all interrupts
        self.disable_interrupts();

        // Do a link and software reset
        self.bar0.ctrl().modify(|_, w| w.lrst(1).rst(1));

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

        // section 4.6.3 - wait for EEPROM auto read completion
        loop {
            let eec = self.bar0.eec().read();
            if eec.auto_rd() == 1 {
                break;
            }
        }

        let mac = self.get_mac_addr();
        info!("initializing device");
        info!(
            "mac address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        );

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
        self.init_rx()?;

        // section 4.6.8 - init tx
        self.init_tx()?;

        enable_bus_master(&mut self.device)?;

        for i in 0..self.num_rx_queues {
            self.start_rx_queue(i)?;
        }

        for i in 0..self.num_tx_queues {
            self.start_tx_queue(i)?;
        }

		// TODO: do I want to be interrupt based?
        //for queue in 0..self.num_rx_queues {
        //    self.enable_interrupt(queue);
        //}

        self.set_promisc(true);

        Ok(())
    }

    fn init_tx(&mut self) -> Result<(), E> {
        info!("Initializing TX");

        // crc offload and small packet padding
        self.bar0.hlreg0().modify(|_, w| w.txcren(1).txpaden(1));

        // section 4.6.11.3.4 - set default buffer size allocations
        self.bar0.txpbsize(0).modify(|_, w| w.size(160));
        for i in 1..8 {
            self.bar0.txpbsize(i).modify(|_, w| w.size(0));
        }

        // required when not using DCB/VTd
        // set the max bytes field to max
        self.bar0
            .dtxmxszrq()
            .modify(|_, w| w.max_bytes_num_req(0xfff));
        self.bar0.rttdcs().modify(|_, w| w.arbdis(0));

        // configure queues
        for i in 0..self.num_tx_queues {
            info!("Initializing TX queue {}", i);
            // section 7.1.9 - setup descriptor ring
            let ring_size_bytes = NUM_TX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();

            info!(
                "Allocating {} bytes for TX queue descriptor ring {}",
                ring_size_bytes, i
            );

            let mut descriptor_mem = DmaMemory::new(ring_size_bytes, &self.dma_space, false)?;

            // initialize to 0xff to prevent rogue memory accesses on premature dma activation
            for i in 0..ring_size_bytes {
                unsafe { core::ptr::write_volatile(descriptor_mem.ptr().add(i), 0xff) }
            }

            self.bar0
                .tdbal(i.into())
                .write(|w| w.tdbal((descriptor_mem.device_addr() & 0xffff_ffff) as u32));
            self.bar0
                .tdbah(i.into())
                .write(|w| w.tdbah((descriptor_mem.device_addr() >> 32) as u32));
            self.bar0
                .tdlen(i.into())
                .write(|w| w.len(ring_size_bytes as u32));

            info!("Set up of DMA for TX descriptor ring {} done", i);

            // Magic performance values from ixy/DPDK
            let pthresh = 36;
            let hthresh = 8;
            let wthresh = 4;
            self.bar0
                .txdctl(i.into())
                .modify(|_, w| w.pthresh(pthresh).hthresh(hthresh).wthresh(wthresh));

            let tx_queue = TxQueue {
                descriptors: descriptor_mem,
                pool: self.rx_queues[i as usize].pool.clone(),
                num_descriptors: NUM_TX_QUEUE_ENTRIES,
                clean_index: 0,
                tx_index: 0,
                bufs_in_use: VecDeque::with_capacity(NUM_TX_QUEUE_ENTRIES as usize),
            };

            self.tx_queues.push(tx_queue);
            info!("TX queue {} initialized", i);
        }

        self.bar0.dmatxctl().modify(|_, w| w.te(1));

        info!("TX initialization done");
        Ok(())
    }

    fn init_rx(&mut self) -> Result<(), E> {
        info!("Initializing RX");

        // disable rx while re-configuring it
        self.bar0.rxctrl().modify(|_, w| w.rxen(0));

        // section 4.6.11.3.4 - allocate all queues and traffic to PB0
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

            // enable advanced rx descriptors
            // let nic drop packets if no rx descriptor is available instead of buffering them
            self.bar0
                .srrctl(i.into())
                .modify(|_, w| w.desctype(SRRCTL_DESCTYPE_ADV_ONE_BUFFER).drop_en(1));

            // section 7.1.9 - setup descriptor ring
            let ring_size_bytes = NUM_RX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();

            info!(
                "Allocating {} bytes for RX queue descriptor ring {}",
                ring_size_bytes, i
            );

            let mut descriptor_mem = DmaMemory::new(ring_size_bytes, &self.dma_space, false)?;
            // initialize to 0xff to prevent rogue memory accesses on premature dma activation
            for i in 0..ring_size_bytes {
                unsafe { core::ptr::write_volatile(descriptor_mem.ptr().add(i), 0xff) }
            }

            self.bar0
                .rdbal(i.into())
                .write(|w| w.rdbal((descriptor_mem.device_addr() & 0xffff_ffff) as u32));
            self.bar0
                .rdbah(i.into())
                .write(|w| w.rdbah((descriptor_mem.device_addr() >> 32) as u32));
            self.bar0
                .rdlen(i.into())
                .write(|w| w.len(ring_size_bytes as u32));

            // set ring to empty at start
            self.bar0.rdh(i.into()).modify(|_, w| w.rdh(0));
            self.bar0.rdt(i.into()).modify(|_, w| w.rdt(0));

            info!("Set up of DMA for RX descriptor ring {} done", i);

            info!("Setting up RX/TX Mempool {}", i);

            // The sum is used here because we share a mempool between rx and tx queues
            let mempool_entries = NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES;

            let mempool =
                Mempool::new(mempool_entries.into(), PKT_BUF_ENTRY_SIZE, &self.dma_space)?;

            info!("Set up of RX/TX Mempool {} done", i);

            let queue = RxQueue {
                descriptors: descriptor_mem,
                pool: mempool,
                num_descriptors: NUM_RX_QUEUE_ENTRIES,
                rx_index: 0,
                bufs_in_use: Vec::with_capacity(NUM_RX_QUEUE_ENTRIES.into()),
            };

            self.rx_queues.push(queue);
            info!("RX queue {} initialized", i);
        }

        // last sentence of section 4.6.7 - set some magic bits
        self.bar0.ctrl_ext().modify(|_, w| w.ns_dis(1));

        // This reserved field says it wants to be set to 0 but is initially 1
        for i in 0..self.num_rx_queues {
            self.bar0.dca_rxctl(i.into()).modify(|_, w| w.reserved3(0));
        }

        // start rx
        self.bar0.rxctrl().modify(|_, w| w.rxen(1));

        info!("RX initialization done");

        Ok(())
    }

    fn start_rx_queue(&mut self, queue_id: u8) -> Result<(), E> {
        info!("Starting RX queue: {}", queue_id);
        let queue = &mut self.rx_queues[queue_id as usize];

        // Power of 2
        assert!(queue.num_descriptors & (queue.num_descriptors - 1) == 0);

        for i in 0..queue.num_descriptors {
            let descriptor_ptr = queue.nth_descriptor_ptr(i.into());
            let pool = &queue.pool;
            let mempool_idx = pool.alloc_buf().ok_or(Error::MempoolEmpty)?;
            let device_addr = pool.get_device_addr(mempool_idx) as u64;

            let mut desc = dev::Descriptors::adv_rx_desc_read::new(descriptor_ptr);
            desc.pkt_addr().write(|w| w.pkt_addr(device_addr));
            desc.hdr_addr().write(|w| w.hdr_addr(0));

            // we need to remember which descriptor entry belongs to which mempool entry
            queue.bufs_in_use.push(mempool_idx);
        }

        // enable queue and wait if necessary
        self.bar0.rxdctl(queue_id.into()).modify(|_, w| w.enable(1));
        loop {
            let rxdctl = self.bar0.rxdctl(queue_id.into()).read();
            if rxdctl.enable() == 1 {
                break;
            }
        }

        self.bar0.rdh(queue_id.into()).modify(|_, w| w.rdh(0));
        self.bar0
            .rdt(queue_id.into())
            .modify(|_, w| w.rdt(queue.num_descriptors - 1));

        info!("Started RX queue: {}", queue_id);

        Ok(())
    }

    fn start_tx_queue(&mut self, queue_id: u8) -> Result<(), E> {
        info!("Starting TX queue: {}", queue_id);

        let queue = &mut self.tx_queues[queue_id as usize];

        // Power of 2
        assert!(queue.num_descriptors & (queue.num_descriptors - 1) == 0);

        // tx queue starts out empty
        self.bar0.tdh(queue_id.into()).modify(|_, w| w.tdh(0));
        self.bar0.tdt(queue_id.into()).modify(|_, w| w.tdt(0));

        // enable queue and wait if necessary
        self.bar0.txdctl(queue_id.into()).modify(|_, w| w.enable(1));
        loop {
            let txdctl = self.bar0.txdctl(queue_id.into()).read();
            if txdctl.enable() == 1 {
                break;
            }
        }

        info!("Started TX queue: {}", queue_id);

        Ok(())
    }

    fn set_promisc(&mut self, enabled: bool) {
        if enabled {
            info!("enabling promisc mode");
            self.bar0.fctrl().modify(|_, w| w.mpe(1).upe(1));
        } else {
            info!("disabling promisc mode");
            self.bar0.fctrl().modify(|_, w| w.mpe(0).upe(0));
        }
    }

    pub fn read_stats(&mut self, stats: &mut DeviceStats) {
        let rx_pkts = u64::from(self.bar0.gprc().read().gprc());
        let tx_pkts = u64::from(self.bar0.gptc().read().gptc());
        let rx_bytes = u64::from(self.bar0.gorcl().read().cnt_l())
            + (u64::from(self.bar0.gorch().read().cnt_h()) << 32);
        let tx_bytes = u64::from(self.bar0.gotcl().read().cnt_l())
            + (u64::from(self.bar0.gotch().read().cnt_h()) << 32);

        stats.rx_pkts += rx_pkts;
        stats.tx_pkts += tx_pkts;
        stats.rx_bytes += rx_bytes;
        stats.tx_bytes += tx_bytes;
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

        self.bar0.txdpgc().read();
    }

    fn init_link(&mut self) {
        info!("Initialising link");
        // We use SFI and XAUI because ixy does as well, if this fails we can probably conclude this is wrong for our card
        self.bar0
            .autoc()
            .modify(|_, w| w.lms(AUTOC_LMS_10G_SFI).teng_pma_pmd_parallel(0b00));
        // Start negotiation
        self.bar0.autoc().modify(|_, w| w.restart_an(1));
    }

    fn get_link_speed(&mut self) -> Option<u16> {
        let links = self.bar0.links().read();
        if links.link_up() == 0 {
            return None;
        }

        match links.link_speed() {
            0b00 => None,
            LINKS_LINK_SPEED_100M => Some(100),
            LINKS_LINK_SPEED_1G => Some(1000),
            LINKS_LINK_SPEED_10G => Some(10000),
            _ => unreachable!(),
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

    fn set_ivar(&mut self, queue_id: u8) {
        let ivar_idx = (queue_id as usize) / 2;
        // We set up our interrupts such that the MSI-X index maps to the rx_queue_id, hence that is our allocation
        if queue_id % 2 == 0 {
            // even RX queues are at alloc_0
            self.bar0
                .ivar(ivar_idx)
                .modify(|_, w| w.int_alloc0(queue_id).int_alloc_val0(1));
        } else {
            // uneven RX queues are at alloc_2
            self.bar0
                .ivar(ivar_idx)
                .modify(|_, w| w.int_alloc2(queue_id).int_alloc_val2(1));
        }
    }

    fn enable_interrupt(&mut self, rx_queue_id: u8) {
        // We only support MSI-X interrupt setups for now
        info!("Enabling interrupts for RX queue {}", rx_queue_id);

        // Step 1: The software driver associates between interrupt causes and MSI-X vectors and the
        //throttling timers EITR[n] by programming the IVAR[n] and IVAR_MISC registers.
        self.bar0
            .gpie()
            .modify(|_, w| w.multiple_msix(1).pba_support(1).eiame(1));
        self.set_ivar(rx_queue_id);

        // Step 2: Program SRRCTL[n].RDMTS (per receive queue) if software uses the receive
        // descriptor minimum threshold interrupt
        // We don't use the minimum threshold interrupt

        // Step 3: The EIAC[n] registers should be set to auto clear for transmit and receive interrupt
        // causes (for best performance). The EIAC bits that control the other and TCP timer
        // interrupt causes should be set to 0b (no auto clear).
        self.bar0.eiac().modify(|_, w| w.rtxq_autoclear(0xffff));

        // Step 4: Set the auto mask in the EIAM register according to the preferred mode of operation.
        // In our case we prefer to not auto-mask the interrupts

        // Step 5: Set the interrupt throttling in EITR[n] and GPIE according to the preferred mode of operation.
        self.bar0
            .eitr(rx_queue_id.into())
            .modify(|_, w| w.itr_interval(self.interrupts.itr_rate));

        // Step 6: Software enables the required interrupt causes by setting the EIMS register
        self.bar0
            .eims()
            .modify(|r, w| w.interrupt_enable(r.interrupt_enable() | (1 << rx_queue_id)));
        info!("Enabled interrupts for RX queue {}", rx_queue_id);
    }

    fn clear_interrupts(&mut self) {
        // 31 ones, the last bit in the register is reserved
        self.bar0
            .eimc()
            .write(|w| w.interrupt_mask(0b1111111111111111111111111111111));
        self.bar0.eicr().read();
    }

    fn disable_interrupts(&mut self) {
        self.bar0.eims().write(|w| w.interrupt_enable(0x0));
        self.clear_interrupts();
    }

    pub fn rx_batch(
        &mut self,
        queue_id: u8,
        buffer: &mut VecDeque<Packet<E, Dma, MM>>,
        num_packets: usize,
    ) -> usize {
        let mut rx_index;
        let mut last_rx_index;
        let mut received_packets = 0;

        {
            let queue = self
                .rx_queues
                .get_mut(queue_id as usize)
                .expect("invalid rx queue id");

            rx_index = queue.rx_index;
            last_rx_index = queue.rx_index;

            // TODO: wait for interrupt on our queue

            for i in 0..num_packets {
                let descriptor_ptr = queue.nth_descriptor_ptr(rx_index);
                let mut desc = dev::Descriptors::adv_rx_desc_wb::new(descriptor_ptr);
                let upper = desc.upper().read();

                if upper.dd() == 0 {
                    break;
                }

                if upper.eop() != 1 {
                    panic!("increase buffer size or decrease MTU")
                }

                let pool = &queue.pool;

                // TODO: can we get bulk allocation going here?
                // get a free buffer from the mempool
                if let Some(buf) = pool.alloc_buf() {
                    // replace currently used buffer with new buffer
                    let buf = mem::replace(&mut queue.bufs_in_use[rx_index], buf);

                    // TODO: This performs two RefCell and one Rc operation in the hot path
                    // While we do need the addresses at some point it should surely be faster to bulk this
                    let p = Packet {
                        addr_virt: pool.get_our_addr(buf),
                        addr_phys: pool.get_device_addr(buf),
                        len: upper.pkt_len().into(),
                        pool: pool.clone(),
                        pool_entry: buf,
                    };

                    // TODO: pre fetch magics
                    // #[cfg(all(
                    //     any(target_arch = "x86", target_arch = "x86_64"),
                    //     target_feature = "sse"
                    // ))]
                    // p.prefetch(Prefetch::Time1);

                    buffer.push_back(p);

                    let mut desc = dev::Descriptors::adv_rx_desc_read::new(desc.consume());
                    
                    // TODO: This is a second RefCell operation even though we already did the first to obtain the buffer above
                    desc.pkt_addr().write(|w| w.pkt_addr(pool.get_device_addr(queue.bufs_in_use[rx_index]) as u64));
                    desc.hdr_addr().write(|w| w.hdr_addr(0));

                    last_rx_index = rx_index;
                    rx_index = wrap_ring(rx_index, queue.num_descriptors.into());
                    received_packets = i + 1;
                } else {
                    // break if there was no free buffer
                    break;
                }
            }

            // TODO: More interrupt stuff
            //let interrupt = &mut self.interrupts.queues[queue_id as usize];
            //let int_en = interrupt.interrupt_enabled;
            //interrupt.rx_pkts += received_packets as u64;

            //interrupt.instr_counter += 1;
            //if (interrupt.instr_counter & 0xFFF) == 0 {
            //    interrupt.instr_counter = 0;
            //    let elapsed = interrupt.last_time_checked.elapsed();
            //    let diff =
            //        elapsed.as_secs() * 1_000_000_000 + u64::from(elapsed.subsec_nanos());
            //    if diff > interrupt.interval {
            //        interrupt.check_interrupt(diff, received_packets, num_packets);
            //    }

            //    if int_en != interrupt.interrupt_enabled {
            //        if interrupt.interrupt_enabled {
            //            self.enable_interrupt(queue_id).unwrap();
            //        } else {
            //            self.disable_interrupt(queue_id);
            //        }
            //    }
            //}
        }

        if rx_index != last_rx_index {
            self.bar0.rdt(queue_id.into()).modify(|_, w| w.rdt(last_rx_index as u16));
            self.rx_queues[queue_id as usize].rx_index = rx_index;
        }

        received_packets
    } 

    pub fn tx_batch_busy_wait(&mut self, queue_id: u16, buffer: &mut VecDeque<Packet<E, Dma, MM>>) {
        while !buffer.is_empty() {
            self.tx_batch(queue_id, buffer);
        }
    }

    pub fn tx_batch(&mut self, queue_id: u16, buffer: &mut VecDeque<Packet<E, Dma, MM>>) -> usize {
        let mut sent = 0;

        let mut queue = self
            .tx_queues
            .get_mut(queue_id as usize)
            .expect("invalid tx queue id");

        let mut cur_index = queue.tx_index;
        let clean_index = clean_tx_queue(&mut queue);

        while let Some(packet) = buffer.pop_front() {
            assert!(
                Rc::ptr_eq(&queue.pool, &packet.pool),
                "distinct memory pools for a single tx queue are not supported yet"
            );

            let next_index = wrap_ring(cur_index, queue.num_descriptors.into());

            if clean_index == next_index {
                // tx queue of device is full, push packet back onto the
                // queue of to-be-sent packets
                buffer.push_front(packet);
                break;
            }

            queue.tx_index = wrap_ring(queue.tx_index, queue.num_descriptors.into());

            let descriptor_ptr = queue.nth_descriptor_ptr(cur_index);
            let mut desc = dev::Descriptors::adv_tx_desc_read::new(descriptor_ptr);
            desc.lower().write(|w| w.address(packet.addr_phys as u64));
            desc.upper().write(|w| {
                w.paylen(packet.len as u32)
                    .dtalen(packet.len as u16)
                    .eop(1)
                    .rs(1)
                    .ifcs(1)
                    .dext(1)
                    .dtyp(ADV_TX_DESC_DTYP_DATA)
            });

            queue.bufs_in_use.push_back(packet.pool_entry);

            // Here we purposely don't drop the packet because the memory would be returned to the mempool right away.
            // Instead this will happen once we clean the tx queue.
            mem::forget(packet);

            cur_index = next_index;
            sent += 1;
        }

        self.bar0
            .tdt(queue_id.into())
            .write(|w| w.tdt(self.tx_queues[queue_id as usize].tx_index as u16));

        sent
    }
}

const TX_CLEAN_BATCH: usize = 32;

fn clean_tx_queue<E, Dma, MM>(queue: &mut TxQueue<E, Dma, MM>) -> usize
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    let mut clean_index = queue.clean_index;
    let cur_index = queue.tx_index;

    loop {
        let mut cleanable = cur_index as i32 - clean_index as i32;

        if cleanable < 0 {
            cleanable += queue.num_descriptors as i32;
        }

        if cleanable < TX_CLEAN_BATCH as i32 {
            break;
        }

        let mut cleanup_to = clean_index + TX_CLEAN_BATCH - 1;

        if cleanup_to >= queue.num_descriptors.into() {
            cleanup_to -= queue.num_descriptors as usize;
        }

        let descriptor_ptr = queue.nth_descriptor_ptr(cleanup_to);
        let mut desc = dev::Descriptors::adv_tx_desc_wb::new(descriptor_ptr);
        let status = desc.lower().read().dd();

        if status != 0 {
            if TX_CLEAN_BATCH as usize >= queue.bufs_in_use.len() {
                queue.pool.free_chunk(queue.bufs_in_use.drain(..));
            } else {
                queue.pool.free_chunk(queue.bufs_in_use.drain(..TX_CLEAN_BATCH));
            }

            clean_index = wrap_ring(cleanup_to, queue.num_descriptors.into());
        } else {
            break;
        }
    }

    queue.clean_index = clean_index;

    clean_index
}

fn wrap_ring(index: usize, ring_size: usize) -> usize {
    (index + 1) & (ring_size - 1)
}

impl DeviceStats {
    ///  Prints the stats differences between `stats_old` and `self`.
    pub fn print_stats_diff(&self, stats_old: &DeviceStats, nanos: u64) {
        let mbits = self.diff_mbit(
            self.rx_bytes,
            stats_old.rx_bytes,
            self.rx_pkts,
            stats_old.rx_pkts,
            nanos,
        );
        let mpps = self.diff_mpps(self.rx_pkts, stats_old.rx_pkts, nanos);
        info!("RX: {:.2} Mbit/s {:.2} Mpps", mbits, mpps);

        let mbits = self.diff_mbit(
            self.tx_bytes,
            stats_old.tx_bytes,
            self.tx_pkts,
            stats_old.tx_pkts,
            nanos,
        );
        let mpps = self.diff_mpps(self.tx_pkts, stats_old.tx_pkts, nanos);
        info!("TX: {:.2} Mbit/s {:.2} Mpps", mbits, mpps);
    }

    /// Returns Mbit/s between two points in time.
    fn diff_mbit(
        &self,
        bytes_new: u64,
        bytes_old: u64,
        pkts_new: u64,
        pkts_old: u64,
        nanos: u64,
    ) -> f64 {
        ((bytes_new - bytes_old) as f64 / 1_000_000.0 / (nanos as f64 / 1_000_000_000.0))
            * f64::from(8)
            + self.diff_mpps(pkts_new, pkts_old, nanos) * f64::from(20) * f64::from(8)
    }

    /// Returns Mpps between two points in time.
    fn diff_mpps(&self, pkts_new: u64, pkts_old: u64, nanos: u64) -> f64 {
        (pkts_new - pkts_old) as f64 / 1_000_000.0 / (nanos as f64 / 1_000_000_000.0)
    }
}
