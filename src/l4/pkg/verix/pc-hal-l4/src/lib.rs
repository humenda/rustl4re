extern crate l4;
extern crate l4_sys;
extern crate l4re;

use l4re::env::get_cap;
use l4re::OwnedCap;
use l4re::mem::VolatileMemoryInterface;
use l4re::io::ResourceType;
use l4re::sys::l4vbus_iface_type_t;
use l4re::factory::{Factory, FactoryCreate, IrqSender};
use l4re::mem::{MaFlags, DsMapFlags, DsAttachFlags};
use l4::cap::Cap;

use pc_hal::traits::Device as _;

// TODO: Dunno if I like this wrapper style
pub struct Vbus(Cap<l4re::io::Vbus>);
pub struct Device(l4re::io::Device);
pub struct PciDevice(Device);
pub struct DmaSpace(OwnedCap<l4re::factory::DmaSpace>);
pub struct Resource(l4re::io::Resource);
pub struct DeviceIter(l4re::io::DeviceIter);
pub struct ResourceIter<'a>(l4re::io::ResourceIter<'a>);
pub struct IoMem(l4re::io::IoMem);
pub struct Icu(OwnedCap<l4re::io::Icu>);
pub struct MappableMemory(l4re::mem::AttachedDataspace);
pub struct IrqHandler {
    irq_cap: OwnedCap<IrqSender>,
    irq_num: u32,
    msi_addr: u64,
    msi_data: u32,
    unmask_icu: bool
}

impl AsMut<Device> for PciDevice {
    fn as_mut(&mut self) -> &mut Device {
        &mut self.0
    }
}


impl Iterator for DeviceIter {
    type Item = Device;

    fn next(&mut self) -> Option<Self::Item> {
        Some(Device(self.0.next()?))
    }
}

impl Iterator for ResourceIter<'_> {
    type Item = Resource;

    fn next(&mut self) -> Option<Self::Item> {
        Some(Resource(self.0.next()?))
    }
}

impl pc_hal::traits::DmaSpace for DmaSpace {
    fn new() -> DmaSpace {
        let default_factory = Factory::default_factory_from_env();
        DmaSpace(default_factory.create(()).unwrap())
    }
}

impl pc_hal::traits::Bus for Vbus {
    type Error = l4::Error;
    type Device = Device;
    type Resource = Resource;
    type DmaSpace = DmaSpace;
    type DeviceIter = DeviceIter;


    fn get() -> Option<Self> {
        Some(Vbus(get_cap("vbus")?))
    }

    fn device_iter(&self) -> Self::DeviceIter {
        DeviceIter(self.0.device_iter())
    }

    fn assign_dma_domain(&self, dma_domain: &mut Self::Resource, dma_space: &mut Self::DmaSpace) -> Result<(), Self::Error> {
        self.0.assign_dma_domain(&mut dma_domain.0, &mut dma_space.0)
    }
}

impl pc_hal::traits::Device for Device {
    type Resource = Resource;
    type ResourceIter<'a> = ResourceIter<'a>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        ResourceIter(self.0.resource_iter())
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        match iface {
            pc_hal::traits::BusInterface::Icu => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_ICU),
            pc_hal::traits::BusInterface::Gpio => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_GPIO),
            pc_hal::traits::BusInterface::Pci => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PCI),
            pc_hal::traits::BusInterface::Pcidev => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PCIDEV),
            pc_hal::traits::BusInterface::PowerManagement => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PM),
            pc_hal::traits::BusInterface::Vbus => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_BUS),
            pc_hal::traits::BusInterface::Generic => self.0.supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_GENERIC),
        }
    }
}

impl pc_hal::traits::Device for PciDevice {
    type Resource = Resource;
    type ResourceIter<'a> = ResourceIter<'a>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        self.0.resource_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.0.supports_interface(iface)
    }
}

impl pc_hal::traits::FailibleMemoryInterface32 for PciDevice {
    type Addr = u32;
    type Error = l4::Error;

    fn read8(&self, reg: u32) -> Result<u8, Self::Error> {
        self.0.0.vbus().pcidev_cfg_read8(&self.0.0, reg)
    }

    fn read16(&self, reg: u32) -> Result<u16, Self::Error> {
        self.0.0.vbus().pcidev_cfg_read16(&self.0.0, reg)
    }

    fn read32(&self, reg: u32) -> Result<u32, Self::Error> {
        self.0.0.vbus().pcidev_cfg_read32(&self.0.0, reg)
    }

    fn write8(&mut self, reg: u32, val: u8) -> Result<(), Self::Error> {
        self.0.0.vbus().pcidev_cfg_write8(&self.0.0, reg, val)
    }

    fn write16(&mut self, reg: u32, val: u16) -> Result<(), Self::Error> {
        self.0.0.vbus().pcidev_cfg_write16(&self.0.0, reg, val)
    }

    fn write32(&mut self, reg: u32, val: u32) -> Result<(), Self::Error> {
        self.0.0.vbus().pcidev_cfg_write32(&self.0.0, reg, val)
    }
}

impl pc_hal::traits::PciDevice for PciDevice {
    type Device = Device;

    fn try_of_device(dev: Self::Device) -> Option<Self> {
        if dev.supports_interface(pc_hal::traits::BusInterface::Pcidev) {
            Some(PciDevice(dev))
        } else {
            None
        }
    }
}

impl pc_hal::traits::Resource for Resource {
    fn start(&mut self) -> usize {
        self.0.start()
    }

    fn end(&mut self) -> usize {
        self.0.end()

    }

    fn typ(&self) -> pc_hal::traits::ResourceType {
        match self.0.typ() {
            ResourceType::Invalid => pc_hal::traits::ResourceType::Invalid,
            ResourceType::Irq => pc_hal::traits::ResourceType::Irq,
            ResourceType::Mem => pc_hal::traits::ResourceType::Mem,
            ResourceType::Port => pc_hal::traits::ResourceType::Port,
            ResourceType::Bus => pc_hal::traits::ResourceType::Bus,
            ResourceType::Gpio => pc_hal::traits::ResourceType::Gpio,
            ResourceType::DmaDomain => pc_hal::traits::ResourceType::DmaDomain,
            ResourceType::Max => unreachable!(),
        }
    }
}

impl pc_hal::traits::MemoryInterface32 for IoMem {
    type Addr = usize;

    fn write8(&mut self, offset: usize, val: u8) {
        self.0.write8(offset, val)
    }

    fn write16(&mut self, offset: usize, val: u16) {
        self.0.write16(offset, val)
    }

    fn write32(&mut self, offset: usize, val: u32) {
        self.0.write32(offset, val)
    }

    fn read8(&self, offset: usize) -> u8 {
        self.0.read8(offset)
    }

    fn read16(&self, offset: usize) -> u16 {
        self.0.read16(offset)
    }

    fn read32(&self, offset: usize) -> u32 {
        self.0.read32(offset)
    }
}

impl pc_hal::traits::MemoryInterface64 for IoMem {
    fn write64(&mut self, offset: usize, val: u64) {
        self.0.write64(offset, val)
    }

    fn read64(&self, offset: usize) -> u64 {
        self.0.read64(offset)
    }
}

impl pc_hal::traits::IoMem for IoMem {
    type Error = l4::Error;

    fn request(phys: u64, size: u64, flags: l4re::io::IoMemFlags) -> Result<Self, Self::Error> {
        Ok(IoMem(l4re::io::IoMem::request(phys, size, flags)?))
    }
}

impl pc_hal::traits::IrqHandler for IrqHandler {
    type Error = l4::Error;

    fn msi_addr(&self) -> u64 {
        self.msi_addr
    }

    fn msi_data(&self) -> u32 {
        self.msi_data
    }

    fn bind_curr_thread(&mut self, label: u64) -> Result<(), Self::Error> {
        self.irq_cap.bind_curr_thread(label)
    }

    fn receive(&mut self) -> Result<(), Self::Error> {
        self.irq_cap.receive()
    }
}

impl pc_hal::traits::Icu for Icu {
    type Error = l4::Error;
    type Device = Device;
    type Bus = Vbus;
    type IrqHandler = IrqHandler;

    fn from_device(bus: &Self::Bus, dev: Self::Device) -> Result<Self, Self::Error> {
        Ok(Icu(l4re::io::Icu::from_device(&bus.0, dev.0)?))
    }

    fn bind_msi(&self, irq_num: u32, target: &mut Self::Device) -> Result<Self::IrqHandler, Self::Error> {
        let default_factory = Factory::default_factory_from_env();
        let irq_cap : OwnedCap<IrqSender> = default_factory.create(())?;
        let (msi_info, unmask_icu) = self.0.bind_msi(&irq_cap, irq_num, &target.0)?;
        Ok(IrqHandler {
            irq_cap,
            irq_num,
            msi_addr: msi_info.addr,
            msi_data: msi_info.data,
            unmask_icu,
        })
    }

    fn unmask(&self, handler: &mut Self::IrqHandler) -> Result<(), Self::Error> {
        if handler.unmask_icu {
            self.0.unmask(handler.irq_num)?;
        } else {
            handler.irq_cap.unmask()?;
        }
        Ok(())
    }

    fn nr_msis(&self) -> Result<u32, Self::Error> {
        Ok(self.0.info()?.nr_msis)
    }
}

impl pc_hal::traits::MemoryInterface32 for MappableMemory {
    type Addr = usize;

    fn write8(&mut self, offset: usize, val: u8) {
        self.0.write8(offset, val)
    }

    fn write16(&mut self, offset: usize, val: u16) {
        self.0.write16(offset, val)
    }

    fn write32(&mut self, offset: usize, val: u32) {
        self.0.write32(offset, val)
    }

    fn read8(&self, offset: usize) -> u8 {
        self.0.read8(offset)
    }

    fn read16(&self, offset: usize) -> u16 {
        self.0.read16(offset)
    }

    fn read32(&self, offset: usize) -> u32 {
        self.0.read32(offset)
    }
}

impl pc_hal::traits::MemoryInterface64 for MappableMemory {
    fn write64(&mut self, offset: usize, val: u64) {
        self.0.write64(offset, val)
    }
    fn read64(&self, offset: usize) -> u64 {
        self.0.read64(offset)
    }
}

impl pc_hal::traits::MappableMemory for MappableMemory {
    type Error = l4::Error;
    type DmaSpace = DmaSpace;

    fn alloc(size: usize, alloc_flags: MaFlags, map_flags: DsMapFlags, attach_flags: DsAttachFlags) -> Result<Self, Self::Error> {
        let mem = l4re::mem::Dataspace::alloc(size, alloc_flags)?;
        Ok(MappableMemory(l4re::mem::Dataspace::attach(mem, map_flags, attach_flags)?))
    }
    fn map_dma(&mut self, target: &Self::DmaSpace) -> Result<usize, Self::Error> {
        self.0.map_dma(&target.0)
    }
}
