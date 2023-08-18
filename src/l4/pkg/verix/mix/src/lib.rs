pub mod constants;
pub mod emulator;
pub mod ix;

#[cfg(test)]
mod tests {
    use crate::emulator::{
        BasicDevice, Bus, Device, DmaSpace, Error, IoMem, MappableMemory, Resource,
    };
    use crate::ix::IxDevice;
    #[test]
    fn test_full() {
        let mut bus = Bus::new(vec![
            Device::Basic(BasicDevice::new(vec![], vec![])),
            Device::Basic(BasicDevice::new(vec![], vec![])),
            Device::Ix(IxDevice::new(0xabcd0000, 0x1)),
        ]);

        verix_lib::find_dev::<Error, Device, IxDevice, Bus, Resource, MappableMemory, DmaSpace, IoMem>(
            &mut bus,
        )
        .unwrap();
    }
}

#[cfg(kani)]
mod tests {
    use crate::constants::pci::BAR0_SIZE;
    use crate::emulator::{
        BasicDevice, Bus, Device, DmaSpace, Error, IoMem, MappableMemory, Resource,
    };
    use crate::ix::IxDevice;
    #[kani::proof]
    #[kani::unwind(3)]
    fn test_full() {
        let bar0_addr : u32 = kani::any::<u32>();
        // There is enough tail space
        kani::assume(bar0_addr <= u32::MAX - BAR0_SIZE as u32);
        // Mem BAR addresses are 16 byte aligned
        kani::assume(bar0_addr % 16 == 0);

        let dma_domain_start : u32 = kani::any::<u32>();
        // There is enough tail space
        kani::assume(dma_domain_start <= u32::MAX - 1);

        // the DMA domain resource (start to start + 1) is outside of the BAR0 resource
        kani::assume((dma_domain_start + 1) < bar0_addr || (bar0_addr + BAR0_SIZE as u32) < dma_domain_start);

        let ix = IxDevice::new(bar0_addr, dma_domain_start);

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
            MappableMemory,
            DmaSpace,
            IoMem,
        >(&mut bus);
        assert!(res.is_ok());
    }
}
