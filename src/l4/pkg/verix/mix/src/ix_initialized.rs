use std::cell::RefCell;

use crate::{emulator::{BasicDevice, Device, Error}, constants::mmio::{DescriptorLen, QueuePointer, IXGBE_RDBAL0, IXGBE_RDBAH0, IXGBE_RDLEN0, IXGBE_RDT0, IXGBE_RDH0}};

pub struct IxInitializedDevice {
    pub(crate) basic: BasicDevice,
}

pub struct InitializedIoMem(RefCell<InitializedIoMemInner>);
pub struct InitializedIoMemInner {
    pub rdbal0: u32,
    pub rdbah0: u32,
    pub rdlen0: DescriptorLen,
    pub rdt0: QueuePointer,
    pub rdh0: QueuePointer,
}

impl IxInitializedDevice {
    pub fn new() -> Self {
        let ifaces = vec![pc_hal::traits::BusInterface::Pcidev];
        IxInitializedDevice { basic: BasicDevice::new(ifaces) }
    }
}

impl InitializedIoMem {
    pub fn new(rdbal0: u32, rdbah0: u32, rdlen0: u32, rdt0: u16, rdh0: u16) -> Self {
        InitializedIoMem(RefCell::new(InitializedIoMemInner {
            rdbal0,
            rdbah0,
            rdlen0: DescriptorLen(rdlen0),
            rdt0: QueuePointer(rdt0.into()),
            rdh0: QueuePointer(rdh0.into()),
        }))
    }
}

impl InitializedIoMemInner {
    fn handle_write32(&mut self, offset: usize, val: u32) {
        panic!("Write!!!");
    }

    fn handle_read32(&mut self, offset: usize) -> u32 {
        match offset {
            IXGBE_RDBAL0 => {
                self.rdbal0
            }
            IXGBE_RDBAH0 => {
                self.rdbah0
            }
            IXGBE_RDLEN0 => {
                self.rdlen0.0
            }
            IXGBE_RDT0 => {
                self.rdt0.0
            }
            IXGBE_RDH0 => {
                self.rdh0.0
            }
            _ => panic!("invalid read offset: 0x{:x}", offset),
        }
    }
}

impl pc_hal::traits::Device for IxInitializedDevice {
    type Resource = <Device as pc_hal::traits::Device>::Resource;
    type ResourceIter<'a> = <Device as pc_hal::traits::Device>::ResourceIter<'a>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        // We assume the iterator has already been consumed
        vec![].into_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.basic.supports_interface(iface)
    }
}

impl pc_hal::traits::FailibleMemoryInterface32 for IxInitializedDevice {
    type Error = Error;
    type Addr = u32;

    fn write8(&mut self, _offset: Self::Addr, _val: u8) -> Result<(), Self::Error> {
        panic!("PCI write to initialized IX device");
    }

    fn write16(&mut self, _offset: Self::Addr, _val: u16) -> Result<(), Self::Error> {
        panic!("PCI write to initialized IX device");
    }

    fn write32(&mut self, _offset: Self::Addr, _val: u32) -> Result<(), Self::Error> {
        panic!("PCI write to initialized IX device");
    }

    fn read8(&self, _offset: Self::Addr) -> Result<u8, Self::Error> {
        panic!("PCI read from initialized IX device");
    }

    fn read16(&self, _offset: Self::Addr) -> Result<u16, Self::Error> {
        panic!("PCI read from initialized IX device");
    }

    fn read32(&self, _offset: Self::Addr) -> Result<u32, Self::Error> {
        panic!("PCI read from initialized IX device");
    }
}

impl pc_hal::traits::PciDevice for IxInitializedDevice {
    type Device = Device;
    type IoMem = InitializedIoMem;

    fn try_of_device(_dev: Self::Device) -> Option<Self> {
        panic!("Can't create initialized IX device from another device");
    }

    fn request_iomem(
        &mut self,
        _phys: u64,
        _size: u64,
        _flags: pc_hal::traits::IoMemFlags,
    ) -> Result<Self::IoMem, Self::Error> {
        panic!("Can't map further IO memory on initialized IX device");
    }
}

impl pc_hal::traits::MemoryInterface for InitializedIoMem {
    unsafe fn write32(&self, offset: usize, val: u32) { self.0.borrow_mut().handle_write32(offset, val) }
    unsafe fn read32(&self, offset: usize) -> u32 { self.0.borrow_mut().handle_read32(offset) }

    unsafe fn write8(&self, _offset: usize, _val: u8) { panic!("only 32 bit"); }
    unsafe fn write16(&self, _offset: usize, _val: u16) { panic!("only 32 bit"); }
    unsafe fn write64(&self, _offset: usize, _val: u64) { panic!("only 32 bit"); }

    unsafe fn read8(&self, _offset: usize) -> u8 { panic!("only 32 bit"); }
    unsafe fn read16(&self, _offset: usize) -> u16 { panic!("only 32 bit"); }
    unsafe fn read64(&self, _offset: usize) -> u64 { panic!("only 32 bit"); }
}
