use std::cell::RefCell;
use std::collections::VecDeque;

use std::time::Duration;
use std::{mem, thread};

use crate::constants::{
    ADV_TX_DESC_DTYP_DATA, AUTOC2_10G_PMA_SERIAL_SFI, AUTOC_LMS_10G_SFI, LINKS_LINK_SPEED_100M,
    LINKS_LINK_SPEED_10G, LINKS_LINK_SPEED_1G, NUM_RX_QUEUE_ENTRIES, NUM_TX_QUEUE_ENTRIES,
    PKT_BUF_ENTRY_SIZE, SRRCTL_DESCTYPE_ADV_ONE_BUFFER, TX_CLEAN_BATCH, WAIT_LIMIT,
};
use crate::dev;
use crate::dma::{DmaMemory, Mempool, Packet};
use crate::types::{
    DeviceStats, Error, InitializedDevice, Result, RxQueue, TxQueue, UninitializedDevice,
};

use log::{info, trace};

use pc_hal::prelude::*;
use pc_hal_util::pci::{enable_bus_master, map_bar};

impl<E, IM, PD, D, Res, Dma> UninitializedDevice<E, IM, PD, D, Res, Dma>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    pub fn new<B>(
        bus: &mut B,
        mut nic: PD,
    ) -> Result<UninitializedDevice<E, IM, PD, D, Res, Dma>, E>
    where
        B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
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

        let bar0_mem: IM = map_bar(&mut nic, 0)?;
        let bar0 = dev::Intel82559ES::Bar0::new(bar0_mem);

        trace!("Assigning DMA Domain");
        let mut dma_space = Dma::new();
        let mut dma_domain = nic
            .resource_iter()
            .filter(|r| r.typ() == ResourceType::DmaDomain)
            .next()
            .unwrap();
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space)?;

        let dev = UninitializedDevice {
            bar0,
            device: nic,
            dma_space,
        };

        Ok(dev)
    }

    pub fn init<MM>(mut self) -> Result<InitializedDevice<E, IM, PD, D, Res, MM, Dma>, E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        let dev = self.reset_and_init()?;
        Ok(dev)
    }

    #[inline(always)]
    #[track_caller]
    fn wait_reg(&self, f: fn(&Self) -> bool) {
        for _ in 0..=WAIT_LIMIT {
            if f(self) {
                return;
            }
            // TODO: make this sleep high enough such that the driver consistently works
            thread::sleep(Duration::from_millis(100));
        }
        panic!("Wait limit exceeded");
    }

    #[inline(always)]
    #[track_caller]
    fn wait_queue_reg(&self, queue_id: u8, f: fn(u8, &Self) -> bool) {
        for _ in 0..=WAIT_LIMIT {
            if f(queue_id, self) {
                return;
            }
            thread::sleep(Duration::from_millis(100));
        }
        panic!("Wait limit exceeded");
    }

    fn reset_and_init<MM>(mut self) -> Result<InitializedDevice<E, IM, PD, D, Res, MM, Dma>, E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        info!("Resetting device");
        // section 4.6.3.1 - disable all interrupts
        self.disable_interrupts();

        // Do a link and software reset
        self.bar0.ctrl().modify(|_, w| w.lrst(1).rst(1));

        // Wait the length that the datasheet tells us to
        thread::sleep(Duration::from_millis(10));

        // section 4.6.3.1 - disable interrupts again after reset
        self.disable_interrupts();

        // section 4.6.3 - wait for EEPROM auto read completion
        self.wait_reg(|s| s.bar0.eec().read().auto_rd() == 1);

        let mac = self.get_mac_addr();
        info!("initializing device");
        info!(
            "mac address: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        );

        // section 4.6.3 - wait for dma initialization done
        self.wait_reg(|s| s.bar0.rdrxctl().read().dmaidone() == 1);

        // section 4.6.4 - initialize link (auto negotiation)
        self.init_link();

        // unlike ixy we temporarly wait for the link right here to check that it works
        self.wait_for_link()?;

        // section 4.6.5 - statistical counters
        // reset-on-read registers, just read them once
        self.reset_stats();

        info!("Setting up RX/TX Mempool");

        // The sum is used here because we share a mempool between rx and tx queues
        let mempool_entries = NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES;
        let mempool = Mempool::new(mempool_entries.into(), PKT_BUF_ENTRY_SIZE, &self.dma_space)?;

        info!("Set up of RX/TX Mempool done");

        // section 4.6.7 - init rx
        let mut rx_queue = self.init_rx()?;

        // section 4.6.8 - init tx
        let mut tx_queue = self.init_tx()?;

        // TODO: is this required?
        enable_bus_master(&mut self.device)?;

        self.start_rx_queue(&mut rx_queue, &mempool)?;
        self.start_tx_queue(&mut tx_queue)?;

        Ok(InitializedDevice {
            bar0: self.bar0,
            device: self.device,
            rx_queue: RefCell::new(rx_queue),
            tx_queue: RefCell::new(tx_queue),
            dma_space: self.dma_space,
            pool: mempool,
        })
    }

    fn init_tx<MM>(&mut self) -> Result<TxQueue<E, Dma, MM>, E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        info!("Initializing TX");

        // crc offload and small packet padding
        self.bar0.hlreg0().modify(|_, w| w.txcren(1).txpaden(1));

        self.bar0.rttdcs().modify(|_, w| w.arbdis(1));
        // section 4.6.11.3.4 - set default buffer size allocations
        self.bar0.txpbsize(0).modify(|_, w| w.size(160));
        for i in 1..8 {
            self.bar0.txpbsize(i).modify(|_, w| w.size(0));
        }

        self.bar0.txpbthresh(0).modify(|_, w| w.thresh(0xa0));
        for i in 1..8 {
            self.bar0.txpbthresh(i).modify(|_, w| w.thresh(0));
        }

        // required when not using DCB/VTd
        // set the max bytes field to max
        self.bar0
            .dtxmxszrq()
            .modify(|_, w| w.max_bytes_num_req(0xfff));
        self.bar0.rttdcs().modify(|_, w| w.arbdis(0));

        // configure queues
        info!("Initializing TX queue");
        // section 7.1.9 - setup descriptor ring
        let ring_size_bytes = NUM_TX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();

        info!(
            "Allocating {} bytes for TX queue descriptor ring",
            ring_size_bytes
        );

        let mut descriptor_mem = DmaMemory::new(ring_size_bytes, &self.dma_space, false)?;

        // initialize to 0xff to prevent rogue memory accesses on premature dma activation
        descriptor_mem.memset(0xff);

        self.bar0
            .tdbal(0)
            .write(|w| w.tdbal((descriptor_mem.device_addr() & 0xffff_ffff) as u32));
        self.bar0
            .tdbah(0)
            .write(|w| w.tdbah((descriptor_mem.device_addr() >> 32) as u32));
        let qlen = NUM_TX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();
        self.bar0.tdlen(0).write(|w| w.len(qlen as u32));

        info!("Set up of DMA for TX descriptor ring done");

        // Magic performance values from ixy/DPDK
        let pthresh = 36;
        let hthresh = 8;
        let wthresh = 4;
        self.bar0
            .txdctl(0)
            .modify(|_, w| w.pthresh(pthresh).hthresh(hthresh).wthresh(wthresh));

        let tx_queue = TxQueue {
            descriptors: descriptor_mem,
            num_descriptors: NUM_TX_QUEUE_ENTRIES,
            clean_index: 0,
            tx_index: 0,
            bufs_in_use: VecDeque::with_capacity(NUM_TX_QUEUE_ENTRIES as usize),
        };

        info!("TX queue initialized");

        self.bar0.dmatxctl().modify(|_, w| w.te(1));

        info!("TX initialization done");
        Ok(tx_queue)
    }

    fn init_rx<MM>(&mut self) -> Result<RxQueue<E, Dma, MM>, E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        info!("Initializing RX");

        // disable rx while re-configuring it
        self.bar0.rxctrl().modify(|_, w| w.rxen(0));

        // section 4.6.11.3.4 - allocate all queues and traffic to PB0
        self.bar0.rxpbsize(0).modify(|_, w| w.size(0x200));
        for i in 1..8 {
            self.bar0.rxpbsize(i).modify(|_, w| w.size(0));
        }

        // enable CRC offloading
        self.bar0.hlreg0().modify(|_, w| w.rxcrstrip(1));
        self.bar0
            .rdrxctl()
            .modify(|_, w| w.crcstrip(1).rscfrstsize(0x0));

        // accept broadcast packets
        self.bar0.fctrl().modify(|_, w| w.bam(1));
        self.set_promisc(true);

        // configure queues, same for all queues
        info!("Initializing RX queue");

        // enable advanced rx descriptors
        // let nic drop packets if no rx descriptor is available instead of buffering them
        self.bar0
            .srrctl(0)
            .modify(|_, w| w.desctype(SRRCTL_DESCTYPE_ADV_ONE_BUFFER).drop_en(1));

        // section 7.1.9 - setup descriptor ring
        let ring_size_bytes = NUM_RX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();

        info!(
            "Allocating {} bytes for RX queue descriptor ring",
            ring_size_bytes
        );

        let mut descriptor_mem = DmaMemory::new(ring_size_bytes, &self.dma_space, false)?;
        // initialize to 0xff to prevent rogue memory accesses on premature dma activation
        descriptor_mem.memset(0xff);

        self.bar0
            .rdbal(0)
            .write(|w| w.rdbal((descriptor_mem.device_addr() & 0xffff_ffff) as u32));
        self.bar0
            .rdbah(0)
            .write(|w| w.rdbah((descriptor_mem.device_addr() >> 32) as u32));
        let qlen = NUM_RX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();
        self.bar0.rdlen(0).write(|w| w.len(qlen as u32));

        // set ring to empty at start
        self.bar0.rdh(0).modify(|_, w| w.rdh(0));
        self.bar0.rdt(0).modify(|_, w| w.rdt(0));

        info!("Set up of DMA for RX descriptor ring done");

        let queue = RxQueue {
            descriptors: descriptor_mem,
            num_descriptors: NUM_RX_QUEUE_ENTRIES,
            rx_index: 0,
            bufs_in_use: Vec::with_capacity(NUM_RX_QUEUE_ENTRIES.into()),
        };

        info!("RX queue initialized");

        // last sentence of section 4.6.7 - set some magic bits
        self.bar0.ctrl_ext().modify(|_, w| w.ns_dis(1));

        // This reserved field says it wants to be set to 0 but is initially 1
        self.bar0.dca_rxctl(0).modify(|_, w| w.reserved3(0));

        // start rx
        self.bar0.rxctrl().modify(|_, w| w.rxen(1));

        info!("RX initialization done");

        Ok(queue)
    }

    fn start_rx_queue<MM>(
        &mut self,
        queue: &mut RxQueue<E, Dma, MM>,
        pool: &Mempool<E, Dma, MM>,
    ) -> Result<(), E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        info!("Starting RX queue");
        // Power of 2
        assert!(queue.num_descriptors & (queue.num_descriptors - 1) == 0);

        for i in 0..queue.num_descriptors {
            let descriptor_ptr = queue.nth_descriptor_ptr(i.into());
            let mempool_idx = pool.alloc_buf().ok_or(Error::MempoolEmpty)?;
            let device_addr = pool.get_device_addr(mempool_idx) as u64;

            let desc = dev::Descriptors::adv_rx_desc_read::new(descriptor_ptr);
            desc.pkt_addr().write(|w| w.pkt_addr(device_addr));
            desc.hdr_addr().write(|w| w.hdr_addr(0));

            // we need to remember which descriptor entry belongs to which mempool entry
            queue.bufs_in_use.push(mempool_idx);
        }
        // enable queue and wait if necessary
        self.bar0.rxdctl(0).modify(|_, w| w.enable(1));
        self.wait_queue_reg(0, |q, s| s.bar0.rxdctl(q.into()).read().enable() == 1);

        self.bar0.rdh(0).modify(|_, w| w.rdh(0));
        self.bar0
            .rdt(0)
            .modify(|_, w| w.rdt(queue.num_descriptors - 1));

        info!("Started RX queue");

        Ok(())
    }

    fn start_tx_queue<MM>(&mut self, queue: &mut TxQueue<E, Dma, MM>) -> Result<(), E>
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    {
        info!("Starting TX queue");

        // Power of 2
        assert!(queue.num_descriptors & (queue.num_descriptors - 1) == 0);

        // tx queue starts out empty
        self.bar0.tdh(0).modify(|_, w| w.tdh(0));
        self.bar0.tdt(0).modify(|_, w| w.tdt(0));

        // enable queue and wait if necessary
        self.bar0.txdctl(0).modify(|_, w| w.enable(1));
        self.wait_queue_reg(0, |q, s| s.bar0.txdctl(q.into()).read().enable() == 1);

        info!("Started TX queue");

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
        self.bar0
            .autoc()
            .modify(|_, w| w.lms(AUTOC_LMS_10G_SFI).teng_pma_pmd_parallel(0b00));
        self.bar0
            .autoc2()
            .modify(|_, w| w.teng_pma_pmd_serial(AUTOC2_10G_PMA_SERIAL_SFI));
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
        info!("Waiting for link");
        self.wait_reg(|s| s.bar0.links().read().link_up() == 1);

        if let Some(s) = self.get_link_speed() {
            info!("link speed is {} Mbit/s", s);
            Ok(())
        } else {
            Err(Error::LinkDown)
        }
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
        // 31 1s, the last bit in the register is reserved
        self.bar0
            .eimc()
            .write(|w| w.interrupt_mask(0b1111111111111111111111111111111));
    }

    fn disable_interrupts(&mut self) {
        self.clear_interrupts();
    }
}

impl<E, IM, PD, D, Res, Dma, MM> InitializedDevice<E, IM, PD, D, Res, MM, Dma>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    pub fn rx_batch<'a>(
        &'a self,
        buffer: &mut VecDeque<Packet<'a, E, Dma, MM>>,
        num_packets: usize,
    ) -> usize {
        let mut rx_index;
        let mut last_rx_index;
        let mut received_packets = 0;

        let mut queue = self.rx_queue.borrow_mut();
        let pool = &self.pool;

        rx_index = queue.rx_index;
        last_rx_index = queue.rx_index;

        // TODO: wait for interrupt on our queue

        for i in 0..num_packets {
            let descriptor_ptr = queue.nth_descriptor_ptr(rx_index);
            let desc = dev::Descriptors::adv_rx_desc_wb::new(descriptor_ptr);
            let upper = desc.upper().read();

            if upper.dd() == 0 {
                break;
            }

            if upper.eop() != 1 {
                panic!("increase buffer size or decrease MTU")
            }

            // TODO: can we get bulk allocation going here?
            // get a free buffer from the mempool
            if let Some(buf) = pool.alloc_buf() {
                // replace currently used buffer with new buffer
                let buf = mem::replace(&mut queue.bufs_in_use[rx_index], buf);

                let p = Packet::new(&pool, buf, upper.pkt_len().into());

                buffer.push_back(p);

                let desc = dev::Descriptors::adv_rx_desc_read::new(desc.consume());

                // TODO: This is a second RefCell operation even though we already did the first to obtain the buffer above
                desc.pkt_addr().write(|w| {
                    w.pkt_addr(pool.get_device_addr(queue.bufs_in_use[rx_index]) as u64)
                });
                desc.hdr_addr().write(|w| w.hdr_addr(0));

                last_rx_index = rx_index;
                rx_index = wrap_ring(rx_index, queue.num_descriptors.into());
                received_packets = i + 1;
            } else {
                // break if there was no free buffer
                break;
            }
        }

        let queue_id = 0;
        if rx_index != last_rx_index {
            self.bar0
                .rdt(queue_id)
                .modify(|_, w| w.rdt(last_rx_index as u16));
            queue.rx_index = rx_index;
        }

        received_packets
    }

    pub fn tx_batch<'a>(
        &'a self,
        buffer: &mut VecDeque<Packet<'a, E, Dma, MM>>,
        num_packets: usize,
    ) -> usize {
        let mut sent = 0;

        let mut queue = self.tx_queue.borrow_mut();

        let mut cur_index = queue.tx_index;
        let pool = &self.pool;

        let clean_index = clean_tx_queue(&mut queue, pool);

        while let Some(mut packet) = buffer.pop_front() {
            // TODO
            //assert!(
            //    queue.contains(&packet),
            //    "distinct memory pools for a single tx queue are not supported yet"
            //);

            let next_index = wrap_ring(cur_index, queue.num_descriptors.into());

            if clean_index == next_index {
                // tx queue of device is full, push packet back onto the
                // queue of to-be-sent packets
                buffer.push_front(packet);
                break;
            }

            queue.tx_index = wrap_ring(queue.tx_index, queue.num_descriptors.into());

            let descriptor_ptr = queue.nth_descriptor_ptr(cur_index);
            let desc = dev::Descriptors::adv_tx_desc_read::new(descriptor_ptr);
            desc.lower()
                .write(|w| w.address(packet.get_device_addr() as u64));
            desc.upper().write(|w| {
                w.paylen(packet.len() as u32)
                    // TODO: Maybe change this to PKT_BUF_ENTRY_SIZE, I need hardware access to
                    // verify this is correct
                    .dtalen(packet.len() as u16)
                    .eop(1)
                    .rs(1)
                    .ifcs(1)
                    .dext(1)
                    .dtyp(ADV_TX_DESC_DTYP_DATA)
            });

            queue.bufs_in_use.push_back(packet.get_pool_entry());

            // Here we purposely don't drop the packet because the memory would be returned to the mempool right away.
            // Instead this will happen once we clean the tx queue.
            mem::forget(packet);

            cur_index = next_index;
            sent += 1;

            if sent == num_packets {
                break;
            }
        }

        let queue_id = 0;
        self.bar0
            .tdt(queue_id)
            .write(|w| w.tdt(queue.tx_index as u16));

        sent
    }

    pub fn read_stats(&self, stats: &mut DeviceStats) {
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
}

fn clean_tx_queue<E, Dma, MM>(queue: &mut TxQueue<E, Dma, MM>, pool: &Mempool<E, Dma, MM>) -> usize
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
        let desc = dev::Descriptors::adv_tx_desc_wb::new(descriptor_ptr);
        let status = desc.lower().read().dd();

        if status != 0 {
            if TX_CLEAN_BATCH as usize >= queue.bufs_in_use.len() {
                pool.free_chunk(queue.bufs_in_use.drain(..));
            } else {
                pool.free_chunk(queue.bufs_in_use.drain(..TX_CLEAN_BATCH));
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
