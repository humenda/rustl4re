use bitflags::bitflags;
use std::ptr;

pub trait DmaSpace {
    fn new() -> Self;
}

pub trait Bus {
    type Error;
    type Device: Device;
    type Resource: Resource;
    type DmaSpace: DmaSpace;
    type DeviceIter: Iterator<Item = Self::Device>;

    fn get() -> Option<Self>
    where
        Self: Sized;
    fn device_iter(&mut self) -> Self::DeviceIter;

    fn assign_dma_domain(
        &mut self,
        dma_domain: &mut Self::Resource,
        dma_space: &mut Self::DmaSpace,
    ) -> Result<(), Self::Error>;
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum BusInterface {
    Icu,
    Gpio,
    Pci,
    Pcidev,
    PowerManagement,
    Vbus,
    Generic,
}

pub trait Device {
    type Resource: Resource;

    type ResourceIter<'a>: Iterator<Item = Self::Resource>
    where
        Self: 'a;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a>;
    fn supports_interface(&self, iface: BusInterface) -> bool;
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ResourceType {
    Invalid,
    Irq,
    Mem,
    Port,
    Bus,
    Gpio,
    DmaDomain,
}

pub trait Resource {
    fn start(&mut self) -> usize;
    fn end(&mut self) -> usize;
    fn typ(&self) -> ResourceType;
}

// With never type this might be compatible with FailibleMemoryInterface in a cool way
pub trait FailibleMemoryInterface32 {
    type Error;
    type Addr;

    fn write8(&mut self, offset: Self::Addr, val: u8) -> Result<(), Self::Error>;
    fn write16(&mut self, offset: Self::Addr, val: u16) -> Result<(), Self::Error>;
    fn write32(&mut self, offset: Self::Addr, val: u32) -> Result<(), Self::Error>;
    fn read8(&self, offset: Self::Addr) -> Result<u8, Self::Error>;
    fn read16(&self, offset: Self::Addr) -> Result<u16, Self::Error>;
    fn read32(&self, offset: Self::Addr) -> Result<u32, Self::Error>;
}

pub trait RawMemoryInterface {
    fn ptr(&mut self) -> *mut u8;
}
pub trait MemoryInterface {
    unsafe fn write8(&mut self, offset: usize, val: u8);
    unsafe fn write16(&mut self, offset: usize, val: u16);
    unsafe fn write32(&mut self, offset: usize, val: u32);
    unsafe fn write64(&mut self, offset: usize, val: u64);
    unsafe fn read8(&mut self, offset: usize) -> u8;
    unsafe fn read16(&mut self, offset: usize) -> u16;
    unsafe fn read32(&mut self, offset: usize) -> u32;
    unsafe fn read64(&mut self, offset: usize) -> u64;
}

impl<T: RawMemoryInterface> MemoryInterface for T {
    unsafe fn write8(&mut self, offset: usize, val: u8) { ptr::write_volatile(self.ptr().add(offset) as *mut _, val) }
    unsafe fn write16(&mut self, offset: usize, val: u16) { ptr::write_volatile(self.ptr().add(offset) as *mut _, val) }
    unsafe fn write32(&mut self, offset: usize, val: u32) { ptr::write_volatile(self.ptr().add(offset) as *mut _, val) }
    unsafe fn write64(&mut self, offset: usize, val: u64) { ptr::write_volatile(self.ptr().add(offset) as *mut _, val) }

    unsafe fn read8(&mut self, offset: usize) -> u8 { ptr::read_volatile(self.ptr().add(offset) as *const _) }
    unsafe fn read16(&mut self, offset: usize) -> u16 { ptr::read_volatile(self.ptr().add(offset) as *const _) }
    unsafe fn read32(&mut self, offset: usize) -> u32 { ptr::read_volatile(self.ptr().add(offset) as *const _) }
    unsafe fn read64(&mut self, offset: usize) -> u64 { ptr::read_volatile(self.ptr().add(offset) as *const _) }
}

impl RawMemoryInterface for *mut u8 {
    fn ptr(&mut self) -> *mut u8 {
        self.clone()
    }
}

pub trait PciDevice: Device + FailibleMemoryInterface32<Addr = u32> {
    type Device: Device;
    type IoMem: MemoryInterface;

    fn try_of_device(dev: Self::Device) -> Option<Self>
    where
        Self: Sized;

    fn request_iomem(
        &mut self,
        phys: u64,
        size: u64,
        flags: IoMemFlags,
    ) -> Result<Self::IoMem, Self::Error>;
}

bitflags! {
    pub struct IoMemFlags : u8 {
        const NONCACHED = 1 << 1;
        const CACHED = 1 << 2;
        const USE_MTRR = 1 << 3;
        const EAGER_MAP = 1 << 4;
    }
}

pub trait IrqHandler {
    type Error;

    fn msi_addr(&self) -> u64;
    fn msi_data(&self) -> u32;
    fn bind_curr_thread(&mut self, label: u64) -> Result<(), Self::Error>;
    fn receive(&mut self) -> Result<(), Self::Error>;
}

pub trait Icu {
    type Error;
    type Device: Device;
    type Bus: Bus;
    type IrqHandler: IrqHandler;

    fn from_device(bus: &Self::Bus, dev: Self::Device) -> Result<Self, Self::Error>
    where
        Self: Sized;
    fn bind_msi(
        &self,
        irq_num: u32,
        target: &mut Self::Device,
    ) -> Result<Self::IrqHandler, Self::Error>;
    fn unmask(&self, handler: &mut Self::IrqHandler) -> Result<(), Self::Error>;
    fn nr_msis(&self) -> Result<u32, Self::Error>;
}

bitflags! {
    pub struct MaFlags : u8 {
        const CONTINUOUS = 1 << 1;
        const PINNED = 1 << 2;
        const SUPER_PAGES = 1 << 3;
    }
}

bitflags! {
    pub struct DsMapFlags : u16 {
        const R = 1 << 1;
        const W = 1 << 2;
        const X = 1 << 3;
        const RW = 1 << 4;
        const RX = 1 << 5;
        const RWX = 1 << 6;
        const RIGHTS_MASK = 1 << 7;
        const NORMAL = 1 << 8;
        const BUFFERABLE = 1 << 9;
        const UNCACHEABLE = 1 << 10;
        const CACHING_MASK = 1 << 11;
    }
}

bitflags! {
    pub struct DsAttachFlags : u8 {
        const SEARCH_ADDR =  1 << 1;
        const IN_AREA = 1 << 2;
        const EAGER_MAP = 1 << 3;
    }
}

pub trait MappableMemory: RawMemoryInterface {
    type Error;
    // Very unclear if this should be associated types, makes it basically impossible to use
    // generically
    type DmaSpace: DmaSpace;

    // TODO: this is very l4 specific but it should be fine for our purposes
    fn alloc(
        size: usize,
        alloc_flags: MaFlags,
        map_flags: DsMapFlags,
        attach_flags: DsAttachFlags,
    ) -> Result<Self, Self::Error>
    where
        Self: Sized;
    fn map_dma(&mut self, target: &Self::DmaSpace) -> Result<usize, Self::Error>;
}
