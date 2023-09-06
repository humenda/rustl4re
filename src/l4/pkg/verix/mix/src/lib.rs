pub mod constants;
pub mod emulator;
pub mod ix;

#[cfg(test)]
mod unit {
    use crate::emulator::{
        Bus, DmaSpace, MappableMemory
    };
    use crate::ix::{IxDevice, IoMemInner, IoMem};
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
        let mut dma_domain = ix.basic.resources[1].clone();
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space).unwrap();

        let dev = verix_lib::types::UninitializedDevice {
            bar0,
            device: ix,
            dma_space,
        };

        let res = dev.init::<MappableMemory>();
        assert!(res.is_ok());
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
            Device::Basic(BasicDevice::new(vec![], vec![])),
            Device::Basic(BasicDevice::new(vec![], vec![])),
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
    #[kani::unwind(1025)]
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
        let mut dma_domain = ix.basic.resources[1].clone();
        bus.assign_dma_domain(&mut dma_domain, &mut dma_space).unwrap();

        let dev = verix_lib::types::UninitializedDevice {
            bar0,
            device: ix,
            dma_space,
        };

        let res = dev.init::<MappableMemory>();
        assert!(res.is_ok());
    }
/*
    #[kani::proof]
    #[kani::unwind(1024)]
    fn test_condition() {
        let mut x1 : Vec<usize> = (1..1024).collect();
        let x2 : Vec<usize> = x1.drain(0..400).collect();
        let x3 : Vec<usize> = x1.drain(400..600).collect();

        let check_range = 1..1024;
        assert!(check_range.len() == x1.len() + x2.len() + x3.len());
        for x in check_range {
            let x1elem = x1.contains(&x);
            let x2elem = x2.contains(&x);
            let x3elem = x3.contains(&x);
            assert!(
                (x1elem && !x2elem && !x3elem) ||
                (!x1elem && x2elem && !x3elem) ||
                (!x1elem && !x2elem && x3elem)
            );
        }
    }
    */

}
