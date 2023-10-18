pub mod constants;
pub mod emulator;
pub mod ix;
pub mod ix_initialized;

#[cfg(kani)]
mod tests {
    use std::collections::VecDeque;
    use std::time::Duration;

    use crate::emulator::{
        BasicDevice, Bus, Device, DmaSpace, Error, MappableMemory, Resource,
    };
    use crate::ix::{IxDevice, IoMemInner, IoMem};
    use crate::ix_initialized::{IxInitializedDevice, InitializedIoMemInner, InitializedIoMem};
    use pc_hal::prelude::*;
    use verix_lib::dma::{DmaMemory, Mempool, SharedPart, alloc_pkt_batch};
    use verix_lib::types::{InitializedDevice, RxQueue, TxQueue};
    use verix_lib::constants::{NUM_RX_QUEUE_ENTRIES, NUM_TX_QUEUE_ENTRIES, PKT_BUF_ENTRY_SIZE, BATCH_SIZE};
    use verix_lib::dev::Intel82559ES::Bar0;
    use std::cell::RefCell;
    use pc_hal::traits::{DsAttachFlags, DsMapFlags, MaFlags};
    use std::mem;

    enum Mode<'a> {
        Assert,
        Assume(&'a Vec<usize>, &'a Mempool<Error, DmaSpace, MappableMemory>),
    }

    fn verify(x : bool, mode: &Mode) {
        match mode {
            Mode::Assert => assert!(x),
            Mode::Assume(_, _) => kani::assume(x)
        }
    }

    mod rx {
        use crate::constants::desc::rx as desc;
        use super::{verify, Mode};
        use verix_lib::constants::NUM_RX_QUEUE_ENTRIES;

        fn valid_adv_read(descq_mem: *const u8, rdh: u16, counter: u16, mode: &Mode) {
            let idx = ((rdh + counter) % NUM_RX_QUEUE_ENTRIES) as usize;
            let offset = idx * std::mem::size_of::<[u64; 2]>();
            let desc_mem = unsafe { descq_mem.add(offset) } as *mut u64;
            let upper = unsafe { desc_mem.add(1).read() };

            // HBA and DD should be 0, this is what upper consists of:
            verify(upper == 0, mode);

            match mode {
                Mode::Assert => {
                    let lower = unsafe { desc_mem.read() };
                    let pba_ptr = lower as *mut u8;
                    let pba_mtu_ptr = unsafe { pba_ptr.add(1500) }; // At least MTU size
                    unsafe { pba_ptr.read() };
                    unsafe { pba_mtu_ptr.read() };
                },
                Mode::Assume(rx_used_bufs, mempool) => {
                    let buf = rx_used_bufs[counter as usize];
                    let buf_ptr = mempool.get_device_addr(buf) as u64;
                    unsafe { desc_mem.write(buf_ptr); }
                }
            }
        }

        fn valid_adv_wb(descq_mem: *const u8, idx: u16, mode: &Mode) {
            let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
            let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
            let upper = desc::adv_wb_upper(unsafe { desc_mem.add(1).read() });
            verify(upper.eop() == true, mode);
            verify(upper.dd() == true, mode);
        }


        pub fn valid_read(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, mode: &Mode) {
            let desc_count = NUM_RX_QUEUE_ENTRIES;
            verify(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

            let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
            verify(addr == descq_mem as u64, mode);

            let mut counter = 0;
            let mut idx = rdh;
            while idx != rdt && counter < NUM_RX_QUEUE_ENTRIES {
                valid_adv_read(descq_mem, rdh, counter, mode);
                counter += 1;
                idx = (idx + 1) % desc_count;
            }
        }

        pub fn valid_wb(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, rx_index: usize, mode: &Mode) {
            let desc_count = NUM_RX_QUEUE_ENTRIES;
            verify(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

            let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
            verify(addr == descq_mem as u64, mode);

            let rx_index = rx_index as u16;
            let mut counter = 0;
            let mut idx = rx_index;
            while idx != rdh && counter < NUM_RX_QUEUE_ENTRIES {
                valid_adv_wb(descq_mem, idx, mode);
                counter += 1;
                idx = (idx + 1) % desc_count;
            }
        }

        pub fn get_valid_queue_state() -> (u16, u16, u32, u16) {
            // Compute a random but valid RX queue state
            let rdh = kani::any::<u16>();
            kani::assume(rdh < NUM_RX_QUEUE_ENTRIES);
            let rdt = kani::any::<u16>();
            kani::assume(rdt < NUM_RX_QUEUE_ENTRIES);
            let rdlen = (NUM_RX_QUEUE_ENTRIES as usize * std::mem::size_of::<[u64; 2]>()) as u32;
            let rx_index = kani::any::<u16>();
            kani::assume(rx_index < NUM_RX_QUEUE_ENTRIES);
            if rdh < rdt {
                kani::assume(rx_index < rdh || rx_index >= rdt);
            } else {
                kani::assume(rx_index < rdh && rx_index >= rdt);
            }

            (rdh, rdt, rdlen, rx_index)
        }
    }

    mod tx {
        use crate::constants::desc::tx as desc;
        use super::{verify, Mode};
        use verix_lib::constants::NUM_TX_QUEUE_ENTRIES;

        fn valid_adv_read(descq_mem: *const u8, tdh: u16, counter: u16, mode: &Mode) {
            let idx = ((tdh + counter) % NUM_TX_QUEUE_ENTRIES) as usize;
            let offset = idx * std::mem::size_of::<[u64; 2]>();
            let desc_mem = unsafe { descq_mem.add(offset) } as *mut u64;
            let upper = desc::adv_read_upper(unsafe { desc_mem.add(1).read() });
            verify(upper.mac() == 0, mode);
            verify(upper.dtyp() == 0b0011, mode);
            verify(upper.tse() == false, mode);
            verify(upper.vle() == false, mode);
            verify(upper.dext() == true, mode);
            verify(upper.rs() == true, mode);
            verify(upper.ifcs() == true, mode);
            verify(upper.eop() == true, mode);
            // Descriptor Done bit should be 0
            verify(upper.sta() & 0b1 == 0, mode);
            verify(upper.cc() == false, mode);
            verify(upper.popts() == 0, mode);
            verify(upper.paylen() > 0, mode);

            match mode {
                Mode::Assert => {
                    let lower = unsafe { desc_mem.read() };
                    let pba_ptr = lower as *mut u8;
                    // TODO: This is technically an over estimation
                    let pba_mtu_ptr = unsafe { pba_ptr.add(1500) }; // At least MTU size
                    unsafe { pba_ptr.read() };
                    unsafe { pba_mtu_ptr.read() };
                },
                Mode::Assume(tx_used_bufs, mempool) => {
                    let buf = tx_used_bufs[counter as usize];
                    let buf_ptr = mempool.get_device_addr(buf) as u64;
                    unsafe { desc_mem.write(buf_ptr) }
                }
            }
        }

        fn valid_adv_wb(descq_mem: *const u8, idx: u16, mode: &Mode) {
            let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
            let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
            let upper = desc::adv_wb_upper(unsafe { desc_mem.add(1).read() });
            verify(upper.dd() == true, mode);
        }

        pub fn valid_read(descq_mem: *const u8, tdh: u16, tdt: u16, tdlen: u32, tdbal: u32, tdbah: u32, mode: &Mode) {
            let desc_count = NUM_TX_QUEUE_ENTRIES;
            verify(desc_count == (tdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

            let addr : u64 = (tdbal as u64) | ((tdbah as u64) << 32);
            verify(addr == descq_mem as u64, mode);

            let mut counter = 0;
            let mut idx = tdh;
            while idx != tdt && counter < NUM_TX_QUEUE_ENTRIES {
                valid_adv_read(descq_mem, tdh, counter, mode);
                counter += 1;
                idx = (idx + 1) % desc_count;
            }
        }

        pub fn valid_wb(descq_mem: *const u8, tdh: u16, tdt: u16, tdlen: u32, tdbal: u32, tdbah: u32, tx_index: usize, clean_index: usize, mode: &Mode) {
            let desc_count = NUM_TX_QUEUE_ENTRIES;
            verify(desc_count == (tdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

            let addr : u64 = (tdbal as u64) | ((tdbah as u64) << 32);
            verify(addr == descq_mem as u64, mode);

            let tx_index = tx_index as u16;
            // the tx_index always gets synced up with TDT
            verify(tx_index == tdt, mode);

            let clean_index = clean_index as u16;

            let mut counter = 0;
            let mut idx = clean_index;
            while idx != tdh && counter < NUM_TX_QUEUE_ENTRIES {
                valid_adv_wb(descq_mem, idx, mode);
                counter += 1;
                idx = (idx + 1) % desc_count;
            }
        }

        pub fn get_valid_queue_state() -> (u16, u16, u32, u16, u16) {
            // Compute a random but valid TX queue state
            let tdh = kani::any::<u16>();
            kani::assume(tdh < NUM_TX_QUEUE_ENTRIES);
            let tdt = kani::any::<u16>();
            kani::assume(tdt < NUM_TX_QUEUE_ENTRIES);
            let tdlen = (NUM_TX_QUEUE_ENTRIES as usize * std::mem::size_of::<[u64; 2]>()) as u32;
            let tx_index = kani::any::<u16>();
            kani::assume(tx_index < NUM_TX_QUEUE_ENTRIES);
            if tdh < tdt {
                kani::assume(tx_index < tdh || tx_index >= tdt);
            } else {
                kani::assume(tx_index < tdh && tx_index >= tdt);
            }

            // TODO: Additional constraints on clean_index
            let clean_index = kani::any::<u16>();
            kani::assume(clean_index < NUM_TX_QUEUE_ENTRIES);

            (tdh, tdt, tdlen, tx_index, clean_index)
        }
    }

    mod pci {
        use crate::constants::pci::BAR0_SIZE;

        fn get_bar_addr(size: u32) -> u32 {
            let bar_addr : u32 = kani::any::<u32>();
            // There is enough tail space
            kani::assume(bar_addr <= u32::MAX - size);
            // Mem BAR addresses are 16 byte aligned
            kani::assume(bar_addr % 16 == 0);
            bar_addr
        }

        fn get_dma_domain_start() -> u32 {
            let dma_domain_start : u32 = kani::any::<u32>();
            // There is enough tail space
            kani::assume(dma_domain_start <= u32::MAX - 1);
            dma_domain_start
        }

        pub fn get_resource_starts() -> (u32, u32) {
            let bar0_addr = get_bar_addr(BAR0_SIZE as u32);
            let dma_domain_start = get_dma_domain_start();

            // the DMA domain resource (start to start + 1) is outside of the BAR0 resource
            kani::assume((dma_domain_start + 1) < bar0_addr || (bar0_addr + BAR0_SIZE as u32) < dma_domain_start);
            (bar0_addr, dma_domain_start)
        }
    }

    fn get_mac_addr() -> [u8; 6] {
        let addr : [u8; 6] = kani::any();
        kani::assume(!(
            addr[0] == 0xff &&
            addr[1] == 0xff &&
            addr[2] == 0xff &&
            addr[3] == 0xff &&
            addr[4] == 0xff &&
            addr[5] == 0xff
        ));
        addr
    }

    #[kani::proof]
    #[kani::unwind(3)]
    fn test_pci_discovery() {
        let (bar0_addr, dma_domain_start) = pci::get_resource_starts();
        let mac_addr = get_mac_addr();
        let ix = IxDevice::new(bar0_addr, dma_domain_start, kani::any(), kani::any(), kani::any(), kani::any(), kani::any(), mac_addr);

        let mut bus = Bus::new(vec![
            Device::Basic(BasicDevice::new(vec![])),
            Device::Basic(BasicDevice::new(vec![])),
            Device::Ix(ix),
        ]);

        let res = verix_lib::find_dev::<
            Error,
            Device,
            IxDevice,
            Bus,
            Resource,
            DmaSpace,
            IoMem,
        >(&mut bus);
        assert!(res.is_ok());
    }

    enum InitMode {
        DontCare,
        RxNonempty(u16),
        TxNotFull
    }

    fn get_initialized_device(init_mode: InitMode) -> verix_lib::types::InitializedDevice<Error, InitializedIoMem, IxInitializedDevice, Device, Resource, MappableMemory, DmaSpace> {
        let (rdh, rdt, rdlen, rx_index) = rx::get_valid_queue_state();
        let (tdh, tdt, tdlen, tx_index, clean_index) = tx::get_valid_queue_state();

        /*
         * Missing for RX are:
         * - The allocator
         * - The list of used buffers in the RX queue
         * As I understand the key data structure in both is a { xs : Vec<usize> // \forall x \in xs, x < RX_SIZE + TX_SIZE }
         * Furthermore these two datastructure are connected, in particular:
         * 1. Their lengths sum up to RX_SIZE + TX_SIZE
         * 2. Their elements are disjoint
         * Question is: How do we efficiently get such a structure?
         * Ideas:
         * 1. Precise: Sample an arbitrary bijection on the subset of integers that we are interested in
         * 2. Underapproximation: Take the interval 0..(RX_SIZE+TX_SIZE) and split it at the correct point
         * We take the under approximation for now as I don't know how to teach kani about the
         * precise concept nicely
         */

        let queue_len = NUM_RX_QUEUE_ENTRIES + NUM_TX_QUEUE_ENTRIES;
        let num_rx_used = ((rdt + queue_len - rdh) % queue_len) as usize;
        let num_tx_used = ((clean_index + queue_len - tdh) % queue_len) as usize;
        let rx_used_bufs : Vec<usize> = (0..num_rx_used).collect();
        let tx_used_bufs : VecDeque<usize> = (num_rx_used..(num_rx_used + num_tx_used)).collect();
        // TODO: I guess this will cause us to go OOM in some situations
        let free_bufs : Vec<usize> = ((num_rx_used + num_tx_used)..(queue_len as usize)).collect();

        // TODO: get this from the device
        let tx_size = NUM_TX_QUEUE_ENTRIES as usize * mem::size_of::<[u64; 2]>();
        let ma_flags = MaFlags::PINNED | MaFlags::CONTINUOUS;
        let map_flags = DsMapFlags::RW;
        let attach_flags = DsAttachFlags::SEARCH_ADDR | DsAttachFlags::EAGER_MAP;

        let rx_mem = MappableMemory::alloc(rdlen as usize, ma_flags, map_flags, attach_flags).unwrap();
        let rx_dev_addr = rx_mem.ptr() as usize;
        let rx_ptr = rx_mem.ptr();

        let tx_mem = MappableMemory::alloc(tx_size, ma_flags, map_flags, attach_flags).unwrap();
        let tx_dev_addr = tx_mem.ptr() as usize;
        let tx_ptr = rx_mem.ptr();

        let buf_mem_size = queue_len as usize * PKT_BUF_ENTRY_SIZE;
        let buf_mem = MappableMemory::alloc(buf_mem_size, ma_flags |  MaFlags::SUPER_PAGES, map_flags, attach_flags).unwrap();
        let buf_dev_addr = buf_mem.ptr() as usize;

        let rdbal = (rx_ptr as usize & 0xffff_ffff) as u32;
        let rdbah = (rx_ptr as usize >> 32) as u32;
        let tdbal = (tx_ptr as usize & 0xffff_ffff) as u32;
        let tdbah = (tx_ptr as usize >> 32) as u32;

        let device = IxInitializedDevice::new();
        let io_mem = InitializedIoMem::new(rdbal, rdbah, rdlen, rdt, rdh,
                                           tdbal, tdbah, tdlen, tdt, tdh);
        let bar0 = Bar0::new(io_mem);
        let mut dma_space = DmaSpace::new();

        let mempool = Mempool {
            num_entries: queue_len.into(),
            entry_size: PKT_BUF_ENTRY_SIZE,
            shared: RefCell::new(SharedPart {
                mem:DmaMemory {
                    mem: buf_mem,
                    device_addr: buf_dev_addr,
                    size: buf_mem_size,
                },
                free_stack: free_bufs
            })
        };

        match init_mode {
            InitMode::RxNonempty(pkt_len) => {
                kani::assume(rx_index != rdh);
                // TODO duplication with valid_adv_rx_wb
                let idx = rx_index;
                let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
                let desc_mem = unsafe { rx_mem.ptr().add(offset) } as *mut u64;
                let upper = unsafe { desc_mem.add(1) } as *mut u16;
                let pkt_len_ptr = unsafe { upper.add(2) };
                unsafe { pkt_len_ptr.write(pkt_len) }
            }
            InitMode::TxNotFull => {
                kani::assume(tdh != tdt);
            }
            _ => {}
        }

        let rx_index = rx_index as usize;
        let mode = Mode::Assume(&rx_used_bufs, &mempool);
        rx::valid_wb(rx_ptr, rdh, rdt, rdlen, rdbal, rdbah, rx_index, &mode);
        rx::valid_read(rx_ptr, rdh, rdt, rdlen, rdbal, rdbah, &mode);

        let rx_queue = RxQueue {
            descriptors: DmaMemory {
                mem: rx_mem,
                device_addr: rx_dev_addr,
                size: rdlen as usize,
            },
            num_descriptors: NUM_RX_QUEUE_ENTRIES,
            rx_index,
            bufs_in_use: rx_used_bufs
        };

        let tx_queue = TxQueue {
            descriptors: DmaMemory {
                mem: tx_mem,
                device_addr: tx_dev_addr,
                size: tx_size,
            },
            num_descriptors: NUM_TX_QUEUE_ENTRIES,
            clean_index: clean_index.into(),
            tx_index: tx_index.into(),
            bufs_in_use: tx_used_bufs
        };

        let mut dev = InitializedDevice {
            bar0,
            device,
            rx_queue: RefCell::new(rx_queue),
            tx_queue: RefCell::new(tx_queue),
            dma_space,
            pool: mempool
        };

        dev
    }


    fn mock_sleep(_dur: Duration) {

    }

    fn mock_memset<E, Dma, MM>(mem: &mut DmaMemory<E, Dma, MM>, val: u8)
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
        Dma: pc_hal::traits::DmaSpace,
    {
        unsafe { mem.ptr().write_bytes(val, mem.size()); }
    }

    fn mock_clean<E, Dma, MM>(queue: &mut TxQueue<E, Dma, MM>, pool: &Mempool<E, Dma, MM>) -> usize
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
        Dma: pc_hal::traits::DmaSpace,
    {
        return queue.clean_index
    }

    fn loop_limit(limit: u8) -> u8 {
        let lim = kani::any();
        kani::assume(lim < limit);
        lim
    }

    fn setup_dev() -> InitializedDevice<Error, IoMem, IxDevice, Device, Resource, MappableMemory, DmaSpace> {
        let (bar0_addr, dma_domain_start) = pci::get_resource_starts();
        let eec_read_limit : u8 = loop_limit(10);
        let dmaidone_read_limit : u8 = loop_limit(10);
        let links_read_limit : u8 = loop_limit(10);
        let rxdctl0_read_limit : u8 = loop_limit(10);
        let txdctl0_read_limit : u8 = loop_limit(10);

        let mac_addr = get_mac_addr();
        let mut ix = IxDevice::new(bar0_addr, dma_domain_start, eec_read_limit, dmaidone_read_limit, links_read_limit, rxdctl0_read_limit, txdctl0_read_limit, mac_addr);
        // This is equivalent to the state of the bus after device_iter has been called
        let mut bus = Bus::new(vec![]);

        let bar0_mem = IoMem::new(IoMemInner::new(eec_read_limit, dmaidone_read_limit, links_read_limit, rxdctl0_read_limit, txdctl0_read_limit, [0x1, 0x2, 0x3, 0x4, 0x5, 0x6]));
        let bar0 = verix_lib::dev::Intel82559ES::Bar0::new(bar0_mem);

        let mut dma_space = DmaSpace::new();
        let mut dma_domain = crate::emulator::Resource::DmaDomain(ix.dma_dom.clone());
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space).unwrap();

        let dev = verix_lib::types::UninitializedDevice {
            bar0,
            device: ix,
            dma_space,
        };

        let dev = dev.init::<MappableMemory>().unwrap();
        dev
    }

    #[kani::proof]
    #[kani::unwind(33)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup_rx_wb() {
        // TODO: It would be cool if we had a version that doesn't do assertions
        // such that we can skip the validation of the device here since we call setup_dev in
        // multiple locations
        let mut dev = setup_dev();
        let rx_queue = dev.rx_queue.borrow_mut();
        let descq_addr = rx_queue.descriptors.ptr();
        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let rx_index = rx_queue.rx_index;
        rx::valid_wb(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, rx_index, &Mode::Assert);
    }

    #[kani::proof]
    #[kani::unwind(33)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup_rx_read() {
        let mut dev = setup_dev();
        let rx_queue = dev.rx_queue.borrow_mut();
        let descq_addr = rx_queue.descriptors.ptr();
        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let rx_index = rx_queue.rx_index;
        rx::valid_read(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, &Mode::Assert);
        // The read slice is not empty
        assert!(rdh != rdt);
    }

    #[kani::proof]
    #[kani::unwind(33)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup_tx_wb() {
        // TODO: It would be cool if we had a version that doesn't do assertions
        // such that we can skip the validation of the device here since we call setup_dev in
        // multiple locations
        let mut dev = setup_dev();
        let tx_queue = dev.tx_queue.borrow_mut();
        let descq_addr = tx_queue.descriptors.ptr();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();
        let tx_index = tx_queue.tx_index;
        let clean_index = tx_queue.clean_index;
        tx::valid_wb(descq_addr, tdh, tdt, tdlen, tdbal, tdbah, tx_index, clean_index, &Mode::Assert);
    }

    #[kani::proof]
    #[kani::unwind(33)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup_tx_read() {
        let mut dev = setup_dev();
        let tx_queue = dev.tx_queue.borrow_mut();
        let descq_addr = tx_queue.descriptors.ptr();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();
        let tx_index = tx_queue.tx_index;
        tx::valid_read(descq_addr, tdh, tdt, tdlen, tdbal, tdbah, &Mode::Assert);
    }

    #[kani::proof]
    #[kani::unwind(32)]
    fn test_rx_batch_read() {
        let mut dev = get_initialized_device(InitMode::DontCare);
        let mut buffer = VecDeque::with_capacity(BATCH_SIZE);
        let num_rx = dev.rx_batch(&mut buffer, BATCH_SIZE);

        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let mem_ptr = dev.rx_queue.borrow_mut().descriptors.mem.ptr();

        rx::valid_read(mem_ptr, rdh, rdt, rdlen, rdbal, rdbah, &Mode::Assert);

        // Dropping the buffer seems to confuse kani, we thus leak it for now
        mem::forget(buffer);
    }

    #[kani::proof]
    #[kani::unwind(32)]
    fn test_rx_batch_wb() {
        let mut dev = get_initialized_device(InitMode::DontCare);
        let mut buffer = VecDeque::with_capacity(BATCH_SIZE);
        let num_rx = dev.rx_batch(&mut buffer, BATCH_SIZE);
        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let mem_ptr = dev.rx_queue.borrow_mut().descriptors.mem.ptr();
        let rx_index = dev.rx_queue.borrow_mut().rx_index;

        rx::valid_wb(mem_ptr, rdh, rdt, rdlen, rdbal, rdbah, rx_index, &Mode::Assert);

        // Dropping the buffer seems to confuse kani, we thus leak it for now
        mem::forget(buffer);

    }

    #[kani::proof]
    #[kani::unwind(32)]
    // We don't clean in kani as the symex gets confused in the allocator
    #[kani::stub(verix_lib::init::clean_tx_queue, mock_clean)]
    fn test_tx_batch_read() {
        let mut dev = get_initialized_device(InitMode::DontCare);

        let mut pkts = VecDeque::new();
        let pkt_len = kani::any();
        kani::assume(pkt_len < 1500);
        alloc_pkt_batch(&dev.pool, &mut pkts, BATCH_SIZE, pkt_len);

        let num_tx = dev.tx_batch(&mut pkts, BATCH_SIZE);

        let tx_queue = dev.tx_queue.borrow_mut();
        let descq_addr = tx_queue.descriptors.ptr();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();

        tx::valid_read(descq_addr, tdh, tdt, tdlen, tdbal, tdbah, &Mode::Assert);
        mem::forget(pkts);
    }

    #[kani::proof]
    #[kani::unwind(32)]
    // We don't clean in kani as the symex gets confused in the allocator
    #[kani::stub(verix_lib::init::clean_tx_queue, mock_clean)]
    fn test_tx_batch_wb() {
        let mut dev = get_initialized_device(InitMode::DontCare);

        let mut pkts = VecDeque::new();
        let pkt_len = kani::any();
        kani::assume(pkt_len < 1500);
        alloc_pkt_batch(&dev.pool, &mut pkts, BATCH_SIZE, pkt_len);

        let num_tx = dev.tx_batch(&mut pkts, BATCH_SIZE);

        let tx_queue = dev.tx_queue.borrow_mut();
        let descq_addr = tx_queue.descriptors.ptr();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();
        let tx_index = tx_queue.tx_index;
        let clean_index = tx_queue.clean_index;

        tx::valid_wb(descq_addr, tdh, tdt, tdlen, tdbal, tdbah, tx_index, clean_index, &Mode::Assert);
        mem::forget(pkts);
    }

    #[kani::proof]
    #[kani::unwind(32)]
    fn test_rx_batch_nonempty() {
        let pkt_len = kani::any::<u16>();
        kani::assume(pkt_len >= 64);
        kani::assume(pkt_len <= 1500);

        let mut dev = get_initialized_device(InitMode::RxNonempty(pkt_len));

        // This computes the index of the first buffer that we would take a look
        // at to receive a packet. We want to assert later on that this is indeed
        // the index that we received.

        let rx_index = dev.rx_queue.borrow().rx_index;
        let target_buf_idx = dev.rx_queue.borrow().bufs_in_use[rx_index];

        let mut buffer = VecDeque::with_capacity(BATCH_SIZE);
        let num_rx = dev.rx_batch(&mut buffer, BATCH_SIZE);

        // We received precisely BATCH_SIZE packet
        assert!(num_rx == BATCH_SIZE);
        let mut pkt = buffer.pop_back().unwrap();
        // This packet has the expected length
        assert!(pkt.len() == pkt_len as usize);
        // And is at the expected memory location
        assert!(pkt.get_pool_entry() == target_buf_idx);

        // Dropping the buffer seems to confuse kani, we thus leak it for now
        mem::forget(buffer);
        mem::forget(pkt);
    }

    #[kani::proof]
    #[kani::unwind(32)]
    // We don't clean in kani as the symex gets confused in the allocator
    #[kani::stub(verix_lib::init::clean_tx_queue, mock_clean)]
    fn test_tx_batch_nonempty() {
        let mut dev = get_initialized_device(InitMode::DontCare);

        let mut pkts = VecDeque::new();
        let pkt_len = kani::any();
        kani::assume(pkt_len < 1500);
        alloc_pkt_batch(&dev.pool, &mut pkts, BATCH_SIZE, pkt_len);

        let num_tx = dev.tx_batch(&mut pkts, BATCH_SIZE);
        assert!(num_tx == BATCH_SIZE);

        let tx_queue = dev.tx_queue.borrow_mut();
        let descq_addr = tx_queue.descriptors.ptr();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();
        let tx_index = tx_queue.tx_index;
        let clean_index = tx_queue.clean_index;

        tx::valid_wb(descq_addr, tdh, tdt, tdlen, tdbal, tdbah, tx_index, clean_index, &Mode::Assert);
        mem::forget(pkts);
    }
}
