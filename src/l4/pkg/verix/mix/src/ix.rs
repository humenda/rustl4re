use std::cell::RefCell;
use std::rc::Rc;

use verix_lib::dev::VENDOR_ID;
use pc_hal::prelude::_pc_hal_Resource;

use crate::constants::pci::{
    Command, PciState, BAR0_ADDR, COMMAND_ADDR, DEVICE_ID, DEVICE_ID_ADDR, VENDOR_ID_ADDR, BAR_ALIGNMENT, BAR0_SIZE,
};
use crate::emulator::{BasicDevice, Device, Error, IoMem, Resource, BasicResource, DmaDomainResource};

pub struct IxDevice {
    pub(crate) basic: BasicDevice,
    pci_state: PciState,
}

impl IxDevice {
    pub fn new(bar0_addr: u32, dma_domain_start: u32) -> Self {
        // 16 byte aligned
        assert!(bar0_addr % 16 == 0);

        let pci_state = PciState::new(bar0_addr);
        let ifaces = vec![pc_hal::traits::BusInterface::Pcidev];

        let dma_dom = Rc::new(DmaDomainResource {
            start: dma_domain_start as usize,
            end: (dma_domain_start as usize) + 1,
            mapping: RefCell::new(Vec::new())
        });

        let mut bar_res = Resource::Basic(BasicResource {
            start: bar0_addr as usize,
            end: bar0_addr as usize + BAR0_SIZE,
            typ: pc_hal::traits::ResourceType::Mem
        });
        let mut dma_dom_res = Resource::DmaDomain(dma_dom.clone());

        // Resources don't overlap
        assert!(dma_dom_res.end() < bar_res.start() || bar_res.end() < dma_dom_res.start());

        let resources = vec![
            bar_res,
            dma_dom_res
        ];

        Self {
            basic: BasicDevice::new(ifaces, resources),
            pci_state,
        }
    }
}

impl pc_hal::traits::Device for IxDevice {
    type Resource = <Device as pc_hal::traits::Device>::Resource;
    type ResourceIter<'a> = <Device as pc_hal::traits::Device>::ResourceIter<'a>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        self.basic.resource_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.basic.supports_interface(iface)
    }
}

impl pc_hal::traits::FailibleMemoryInterface32 for IxDevice {
    type Error = Error;
    type Addr = u32;

    fn write8(&mut self, offset: Self::Addr, val: u8) -> Result<(), Self::Error> {
        match offset {
            _ => panic!("Invalid pci write8 at: 0x{:x}", offset),
        }
    }

    fn write16(&mut self, offset: Self::Addr, val: u16) -> Result<(), Self::Error> {
        match offset {
            COMMAND_ADDR => {
                let proposed = Command(val);
                // Reserved fields shall stay at their default values
                assert!(proposed.special_cycle() == false);
                assert!(proposed.mwi_en() == false);
                assert!(proposed.palette_en() == false);
                assert!(proposed.wait_en() == false);
                assert!(proposed.fast_en() == false);
                assert!(proposed.reserved0() == 0);

                // Not implemented fields shall stay at their default values
                assert!(proposed.io_en() == false);
                assert!(proposed.parity_error() == false);
                assert!(proposed.serr_en() == false);
                assert!(proposed.interrupt_dis() == true);

                // Command has no effect on write, just take over
                self.pci_state.command = proposed;
            }
            _ => panic!("Invalid pci write16 at: 0x{:x}", offset),
        }
        Ok(())
    }

    fn write32(&mut self, offset: Self::Addr, val: u32) -> Result<(), Self::Error> {
        match offset {
            BAR0_ADDR => {
                if val == self.pci_state.bar0.orig_reg.0 {
                    // The value is reset after obtaining the length
                    self.pci_state.bar0.reg = self.pci_state.bar0.orig_reg.clone();
                } else {
                    // We expect to receive the length masking
                    assert!(val == 0xFFFFFFFF);
                    // memory BAR should be disabled while doing this
                    assert!(self.pci_state.command.mem_en() == false);

                    let encoded_len = !(self.pci_state.bar0.data.len() as u32 - 1);
                    self.pci_state.bar0.reg.set_addr(encoded_len >> 4);
                }
            }
            _ => panic!("Invalid pci write32 at: 0x{:x}", offset),
        }
        Ok(())
    }

    fn read8(&self, offset: Self::Addr) -> Result<u8, Self::Error> {
        match offset {
            _ => panic!("Invalid pci read8 at: 0x{:x}", offset),
        }
    }

    fn read16(&self, offset: Self::Addr) -> Result<u16, Self::Error> {
        let res = match offset {
            VENDOR_ID_ADDR => VENDOR_ID,
            DEVICE_ID_ADDR => DEVICE_ID,
            COMMAND_ADDR => self.pci_state.command.0,
            _ => panic!("Invalid pci read16 at: 0x{:x}", offset),
        };
        Ok(res)
    }

    fn read32(&self, offset: Self::Addr) -> Result<u32, Self::Error> {
        let res = match offset {
            BAR0_ADDR => self.pci_state.bar0.reg.0,
            _ => panic!("Invalid pci read32 at: 0x{:x}", offset),
        };
        Ok(res)
    }
}

impl pc_hal::traits::PciDevice for IxDevice {
    type Device = Device;
    type IoMem = IoMem;

    fn try_of_device(dev: Self::Device) -> Option<Self> {
        match dev {
            Device::Ix(dev) => Some(dev),
            _ => panic!("Attempt to create IxDevice from bad device"),
        }
    }

    fn request_iomem(
        &mut self,
        phys: u64,
        size: u64,
        flags: pc_hal::traits::IoMemFlags,
    ) -> Result<Self::IoMem, Self::Error> {
        let phys: u32 = phys.try_into().unwrap();
        assert!(self.pci_state.bar0.reg.0 == self.pci_state.bar0.orig_reg.0);
        assert!(phys == self.pci_state.bar0.reg.addr() << BAR_ALIGNMENT);
        assert!(size == self.pci_state.bar0.data.len() as u64);
        assert!(flags.contains(pc_hal::traits::IoMemFlags::NONCACHED));
        assert!(!flags.contains(pc_hal::traits::IoMemFlags::CACHED));
        let bar0_ptr = self.pci_state.bar0.data.as_mut_ptr();
        Ok(IoMem { ptr: bar0_ptr })
    }
}
