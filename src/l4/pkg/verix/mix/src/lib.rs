pub mod constants;
pub mod emulator;
pub mod ix;

use verix_lib::{constants::{NUM_RX_QUEUE_ENTRIES, PKT_BUF_ENTRY_SIZE}, dma::Mempool};

fn assert_valid_adv_rx_read(descq_mem: *const u8, idx: u16)
{
    let offset = idx as usize * std::mem::size_of::<[u64; 2]>();
    let desc_mem = unsafe { descq_mem.add(offset) } as *const u64;
    let lower = unsafe { desc_mem.read() };
    let upper = unsafe { desc_mem.add(1).read() };
    // We can read from this pointer as we use an identity mapping for DMA, just like L4
    assert_eq!(upper, 0, "{}", idx);
    let pba_ptr = lower as *mut u8;
    unsafe { pba_ptr.read() };
}

fn assert_valid_rx_queue(descq_mem: *const u8, rdh: u16, rdt: u16, rdlen: u32, rdbal: u32, rdbah: u32, rx_index: usize)
{
    let desc_count = NUM_RX_QUEUE_ENTRIES;
    assert!(desc_count == (rdlen as usize / std::mem::size_of::<[u64; 2]>()) as u16);
    let distance = if rdh < rdt {
        rdt - rdh
    } else {
        desc_count - rdt + rdh
    };
    assert!(distance <= desc_count);
    let mut counter = 0;
    while counter < distance && counter < NUM_RX_QUEUE_ENTRIES {
        let idx = (rdh + counter) % desc_count;
        assert_valid_adv_rx_read(descq_mem, idx);
        counter += 1;
    }
}

#[cfg(test)]
mod unit {
    use crate::emulator::{
        Bus, DmaSpace, MappableMemory
    };
    use crate::ix::{IxDevice, IoMemInner, IoMem};
    use crate::assert_valid_rx_queue;
    use pc_hal::prelude::*;

    fn get_resource_starts() -> (u32, u32) {
        (0xff0000, 0x1)
    }

    #[test]
    fn test_setup() {
        let (bar0_addr, dma_domain_start) = get_resource_starts();
        let eec_read_limit : u8 = 9;
        let dmaidone_read_limit : u8 = 9;
        let links_read_limit : u8 = 9;
        let rxdctl0_read_limit : u8 = 9;
        let txdctl0_read_limit : u8 = 9;

        let ix = IxDevice::new(bar0_addr, dma_domain_start, eec_read_limit, dmaidone_read_limit, links_read_limit, rxdctl0_read_limit, txdctl0_read_limit, [0x1, 0x2, 0x3, 0x4, 0x5, 0x6]);
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
        let rx_queue = dev.rx_queue.borrow_mut();
        let descq_addr = rx_queue.descriptors.ptr();
        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let rx_index = rx_queue.rx_index;
        assert_valid_rx_queue(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, rx_index);
    }
}

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
    use crate::{assert_valid_rx_queue, assert_valid_adv_rx_read};

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

    #[kani::proof]
    #[kani::unwind(257)]
    #[kani::stub(std::thread::sleep, mock_sleep)]
    #[kani::stub(verix_lib::dma::DmaMemory::memset, mock_memset)]
    fn test_setup() {
        let (bar0_addr, dma_domain_start) = get_resource_starts();
        // TODO: make loop counter better
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
        let rx_queue = dev.rx_queue.borrow_mut();
        let descq_addr = rx_queue.descriptors.ptr();
        let rdh = dev.bar0.rdh(0).read().rdh();
        let rdt = dev.bar0.rdt(0).read().rdt();
        let rdlen = dev.bar0.rdlen(0).read().len();
        let rdbal = dev.bar0.rdbal(0).read().rdbal();
        let rdbah = dev.bar0.rdbah(0).read().rdbah();
        let rx_index = rx_queue.rx_index;
        assert_valid_rx_queue(descq_addr, rdh, rdt, rdlen, rdbal, rdbah, rx_index);
    }
}
