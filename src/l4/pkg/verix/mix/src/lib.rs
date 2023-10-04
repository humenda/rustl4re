pub mod constants;
pub mod emulator;
pub mod ix;



#[cfg(kani)]
mod tests {
    use std::time::Duration;

    use crate::constants::pci::BAR0_SIZE;
    use crate::emulator::{
        BasicDevice, Bus, Device, DmaSpace, Error, MappableMemory, Resource,
    };
    use crate::ix::{IxDevice, IoMemInner, IoMem};
    use pc_hal::prelude::*;
    use verix_lib::dma::DmaMemory;
    use verix_lib::types::InitializedDevice;
    use verix_lib::constants::NUM_RX_QUEUE_ENTRIES;

    fn assert_valid_adv_rx_read(descq_mem: *const u8, idx: u16)
    {
        let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
        let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
        let lower = unsafe { desc_mem.read() };
        let upper = unsafe { desc_mem.add(1).read() };
        // We can read from this pointer as we use an identity mapping for DMA, just like L4
        assert_eq!(upper, 0, "{}", idx);
        let pba_ptr = lower as *mut u8;
        let pba_mtu_ptr = unsafe { pba_ptr.add(1500) }; // At least MTU size
        unsafe { pba_ptr.read() };
        unsafe { pba_mtu_ptr.read() };
        // HBA and DD should be 0, this is what upper consists of:
        assert!(upper == 0);
    }

    fn assert_valid_adv_rx_wb(descq_mem: *const u8, idx: u16)
    {
        let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
        let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
        unsafe { desc_mem.read() };
        let upper = unsafe { desc_mem.add(1).read() };
        // EOP and DD are the last two bits and both should be one
        let relevant = upper & 0b11;
        assert!(relevant == 0b11);
        // TODO: correct pkt_len
    }

    fn assert_valid_rx_read(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32) {
        let desc_count = NUM_RX_QUEUE_ENTRIES;
        assert!(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16);

        let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
        assert!(addr == descq_mem as u64);

        let mut counter = 0;
        while rdh + counter < rdt && counter < NUM_RX_QUEUE_ENTRIES {
            let idx = (rdh + counter) % desc_count;
            assert_valid_adv_rx_read(descq_mem, idx);
            counter += 1;
        }
    }

    fn assert_valid_rx_wb(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, rx_index: usize)
    {
        let desc_count = NUM_RX_QUEUE_ENTRIES;
        assert!(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16);

        let addr : u64 = (rdbal as u64) | ((rdbah as u64) << 32);
        assert!(addr == descq_mem as u64);

        let rx_index = rx_index as u16;
        let mut counter = 0;
        while rx_index + counter < rdh && counter < NUM_RX_QUEUE_ENTRIES {
            let idx = (rx_index + counter) % desc_count;
            assert_valid_adv_rx_wb(descq_mem, idx);
            counter += 1;
        }
    }

    fn assert_valid_rx_undefined(rdh: u16, rx_index: usize) {
        let rx_index = rx_index as u16;
        assert!(rdh == rx_index);
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
        assert_valid_rx_wb(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, rx_index);
        assert_valid_rx_undefined(rdh, rx_index);
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
        assert_valid_rx_read(descq_addr, rdh, rdt, rdlen, rdbal, rdbah);
        // The read slice is not empty
        assert!(rdh != rdt);
    }
}
