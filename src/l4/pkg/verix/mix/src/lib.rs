pub mod constants;
pub mod emulator;
pub mod ix;
pub mod ix_initialized;



#[cfg(kani)]
mod tests {
    use std::collections::VecDeque;
    use std::time::Duration;

    use crate::constants::pci::BAR0_SIZE;
    use crate::emulator::{
        BasicDevice, Bus, Device, DmaSpace, Error, MappableMemory, Resource,
    };
    use crate::ix::{IxDevice, IoMemInner, IoMem};
    use crate::ix_initialized::{IxInitializedDevice, InitializedIoMemInner, InitializedIoMem};
    use pc_hal::prelude::*;
    use verix_lib::dma::{DmaMemory, Mempool, SharedPart};
    use verix_lib::types::{InitializedDevice, RxQueue, TxQueue};
    use verix_lib::constants::{NUM_RX_QUEUE_ENTRIES, NUM_TX_QUEUE_ENTRIES, PKT_BUF_ENTRY_SIZE};
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

    fn valid_adv_rx_read(descq_mem: *const u8, rdh: u16, counter: u16, mode: &Mode) {
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

    fn valid_adv_rx_wb(descq_mem: *const u8, idx: u16, mode: &Mode)
    {
        let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
        let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
        unsafe { desc_mem.read() };
        let upper = unsafe { desc_mem.add(1).read() };
        // EOP and DD are the last two bits and both should be one
        let relevant = upper & 0b11;
        verify(relevant == 0b11, mode);
        // TODO: correct pkt_len
    }

    fn valid_rx_read(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, mode: &Mode) {
        let desc_count = NUM_RX_QUEUE_ENTRIES;
        verify(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

        let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
        verify(addr == descq_mem as u64, mode);

        let mut counter = 0;
        let mut idx = rdh;
        while idx != rdt && counter < NUM_RX_QUEUE_ENTRIES {
            valid_adv_rx_read(descq_mem, rdh, counter, mode);
            counter += 1;
            idx = (idx + 1) % desc_count;
        }
    }

    fn valid_rx_wb(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, rx_index: usize, mode: &Mode)
    {
        let desc_count = NUM_RX_QUEUE_ENTRIES;
        verify(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16, mode);

        let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
        verify(addr == descq_mem as u64, mode);

        let rx_index = rx_index as u16;
        let mut counter = 0;
        let mut idx = rx_index;
        while idx != rdh && counter < NUM_RX_QUEUE_ENTRIES {
            valid_adv_rx_wb(descq_mem, idx, mode);
            counter += 1;
            idx = (idx + 1) % desc_count;
        }
    }

    fn valid_rx_undefined(rdh: u16, rx_index: usize, mode: &Mode) {
        let rx_index = rx_index as u16;
        verify(rdh == rx_index, mode);
    }

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

    fn get_resource_starts() -> (u32, u32) {
        let bar0_addr = get_bar_addr(BAR0_SIZE as u32);
        let dma_domain_start = get_dma_domain_start();

        // the DMA domain resource (start to start + 1) is outside of the BAR0 resource
        kani::assume((dma_domain_start + 1) < bar0_addr || (bar0_addr + BAR0_SIZE as u32) < dma_domain_start);
        (bar0_addr, dma_domain_start)
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

    fn get_initialized_device() -> verix_lib::types::InitializedDevice<Error, InitializedIoMem, IxInitializedDevice, Device, Resource, MappableMemory, DmaSpace> {
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
        let rx_index = rx_index as usize;

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
        let rx_used_bufs : Vec<usize> = (0..num_rx_used).collect();
        let free_bufs : Vec<usize> = (num_rx_used..(queue_len as usize)).collect();

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

        let buf_mem_size = queue_len as usize * PKT_BUF_ENTRY_SIZE;
        let buf_mem = MappableMemory::alloc(buf_mem_size, ma_flags |  MaFlags::SUPER_PAGES, map_flags, attach_flags).unwrap();
        let buf_dev_addr = buf_mem.ptr() as usize;

        let rdbal = (rx_ptr as usize & 0xffff_ffff) as u32;
        let rdbah = (rx_ptr as usize >> 32) as u32;

        let device = IxInitializedDevice::new();
        let io_mem = InitializedIoMem::new(rdbal, rdbah, rdlen, rdt, rdh);
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

        let mode = Mode::Assume(&rx_used_bufs, &mempool);
        valid_rx_wb(rx_ptr, rdh, rdt, rdlen, rdbal, rdbah, rx_index, &mode);
        valid_rx_read(rx_ptr, rdh, rdt, rdlen, rdbal, rdbah, &mode);

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
            // TODO: these values need be symbolic
            clean_index: 0,
            tx_index: 0,
            bufs_in_use: VecDeque::new()
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

    #[kani::proof]
    #[kani::unwind(3)]
    fn test_pci_discovery() {
        let (bar0_addr, dma_domain_start) = get_resource_starts();
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


    fn mock_sleep(_dur: Duration) {

    }

    fn mock_memset<E, Dma, MM>(mem: &mut DmaMemory<E, Dma, MM>, val: u8)
    where
        MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
        Dma: pc_hal::traits::DmaSpace,
    {
        unsafe { mem.ptr().write_bytes(val, mem.size()); }
    }

    fn loop_limit(limit: u8) -> u8 {
        let lim = kani::any();
        kani::assume(lim < limit);
        lim
    }

    fn setup_dev() -> InitializedDevice<Error, IoMem, IxDevice, Device, Resource, MappableMemory, DmaSpace> {
        let (bar0_addr, dma_domain_start) = get_resource_starts();
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
    #[kani::unwind(65)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup_rx_wb_and_undefined() {
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
        valid_rx_wb(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, rx_index, &Mode::Assert);
        valid_rx_undefined(rdh, rx_index, &Mode::Assert);
    }

    #[kani::proof]
    #[kani::unwind(65)]
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
        valid_rx_read(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, &Mode::Assert);
        // The read slice is not empty
        assert!(rdh != rdt);
    }

    #[kani::proof]
    #[kani::unwind(64)]
    fn test_assumption() {
        let mut dev = get_initialized_device();

        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let mem_ptr = dev.rx_queue.borrow_mut().descriptors.mem.ptr();

        valid_rx_read(mem_ptr, rdh, rdt, rdlen, rdbal, rdbah, &Mode::Assert);
    }
}
