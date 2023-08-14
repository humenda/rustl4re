extern crate l4;
extern crate l4_sys;
extern crate l4re;

use l4::cap::Cap;
use l4re::env::get_cap;
use l4re::factory::{Factory, FactoryCreate, IrqSender};
use l4re::io::ResourceType;
use l4re::mem::VolatileMemoryInterface;
use l4re::sys::l4vbus_iface_type_t;
use l4re::OwnedCap;

use pc_hal::traits::Device as _;

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
    unmask_icu: bool,
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

    fn device_iter(&mut self) -> Self::DeviceIter {
        DeviceIter(self.0.device_iter())
    }

    fn assign_dma_domain(
        &mut self,
        dma_domain: &mut Self::Resource,
        dma_space: &mut Self::DmaSpace,
    ) -> Result<(), Self::Error> {
        self.0
            .assign_dma_domain(&mut dma_domain.0, &mut dma_space.0)
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
            pc_hal::traits::BusInterface::Icu => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_ICU),
            pc_hal::traits::BusInterface::Gpio => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_GPIO),
            pc_hal::traits::BusInterface::Pci => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PCI),
            pc_hal::traits::BusInterface::Pcidev => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PCIDEV),
            pc_hal::traits::BusInterface::PowerManagement => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_PM),
            pc_hal::traits::BusInterface::Vbus => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_BUS),
            pc_hal::traits::BusInterface::Generic => self
                .0
                .supports_interface(l4vbus_iface_type_t::L4VBUS_INTERFACE_GENERIC),
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
        self.0 .0.vbus().pcidev_cfg_read8(&self.0 .0, reg)
    }

    fn read16(&self, reg: u32) -> Result<u16, Self::Error> {
        self.0 .0.vbus().pcidev_cfg_read16(&self.0 .0, reg)
    }

    fn read32(&self, reg: u32) -> Result<u32, Self::Error> {
        self.0 .0.vbus().pcidev_cfg_read32(&self.0 .0, reg)
    }

    fn write8(&mut self, reg: u32, val: u8) -> Result<(), Self::Error> {
        self.0 .0.vbus().pcidev_cfg_write8(&self.0 .0, reg, val)
    }

    fn write16(&mut self, reg: u32, val: u16) -> Result<(), Self::Error> {
        self.0 .0.vbus().pcidev_cfg_write16(&self.0 .0, reg, val)
    }

    fn write32(&mut self, reg: u32, val: u32) -> Result<(), Self::Error> {
        self.0 .0.vbus().pcidev_cfg_write32(&self.0 .0, reg, val)
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

impl pc_hal::traits::MemoryInterface for IoMem {
    #[inline(always)]
    fn ptr(&mut self) -> *mut u8 {
        self.0.ptr()
    }
}

impl pc_hal::traits::IoMem for IoMem {
    type Error = l4::Error;

    fn request(
        phys: u64,
        size: u64,
        flags: pc_hal::traits::IoMemFlags,
    ) -> Result<Self, Self::Error> {
        let pairs = [
            (
                pc_hal::traits::IoMemFlags::NONCACHED,
                l4re::io::IoMemFlags::NONCACHED,
            ),
            (
                pc_hal::traits::IoMemFlags::CACHED,
                l4re::io::IoMemFlags::CACHED,
            ),
            (
                pc_hal::traits::IoMemFlags::USE_MTRR,
                l4re::io::IoMemFlags::USE_MTRR,
            ),
            (
                pc_hal::traits::IoMemFlags::EAGER_MAP,
                l4re::io::IoMemFlags::EAGER_MAP,
            ),
        ];
        let translated =
            pairs
                .iter()
                .fold(l4re::io::IoMemFlags::empty(), |acc, (pc_flag, l4_flag)| {
                    if (flags.bits() & pc_flag.bits()) != 0 {
                        acc | *l4_flag
                    } else {
                        acc
                    }
                });

        Ok(IoMem(l4re::io::IoMem::request(phys, size, translated)?))
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

    fn bind_msi(
        &self,
        irq_num: u32,
        target: &mut Self::Device,
    ) -> Result<Self::IrqHandler, Self::Error> {
        let default_factory = Factory::default_factory_from_env();
        let irq_cap: OwnedCap<IrqSender> = default_factory.create(())?;
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

impl pc_hal::traits::MemoryInterface for MappableMemory {
    #[inline(always)]
    fn ptr(&mut self) -> *mut u8 {
        self.0.ptr()
    }
}

impl pc_hal::traits::MappableMemory for MappableMemory {
    type Error = l4::Error;
    type DmaSpace = DmaSpace;

    fn alloc(
        size: usize,
        alloc_flags: pc_hal::traits::MaFlags,
        map_flags: pc_hal::traits::DsMapFlags,
        attach_flags: pc_hal::traits::DsAttachFlags,
    ) -> Result<Self, Self::Error> {
        // TODO: dedup
        let pairs_alloc = [
            (
                pc_hal::traits::MaFlags::CONTINUOUS,
                l4re::mem::MaFlags::CONTINUOUS,
            ),
            (pc_hal::traits::MaFlags::PINNED, l4re::mem::MaFlags::PINNED),
            (
                pc_hal::traits::MaFlags::SUPER_PAGES,
                l4re::mem::MaFlags::SUPER_PAGES,
            ),
        ];

        let pairs_map = [
            (pc_hal::traits::DsMapFlags::R, l4re::mem::DsMapFlags::R),
            (pc_hal::traits::DsMapFlags::W, l4re::mem::DsMapFlags::W),
            (pc_hal::traits::DsMapFlags::X, l4re::mem::DsMapFlags::X),
            (pc_hal::traits::DsMapFlags::RW, l4re::mem::DsMapFlags::RW),
            (pc_hal::traits::DsMapFlags::RX, l4re::mem::DsMapFlags::RX),
            (pc_hal::traits::DsMapFlags::RWX, l4re::mem::DsMapFlags::RWX),
            (
                pc_hal::traits::DsMapFlags::RIGHTS_MASK,
                l4re::mem::DsMapFlags::RIGHTS_MASK,
            ),
            (
                pc_hal::traits::DsMapFlags::NORMAL,
                l4re::mem::DsMapFlags::NORMAL,
            ),
            (
                pc_hal::traits::DsMapFlags::BUFFERABLE,
                l4re::mem::DsMapFlags::BUFFERABLE,
            ),
            (
                pc_hal::traits::DsMapFlags::UNCACHEABLE,
                l4re::mem::DsMapFlags::UNCACHEABLE,
            ),
            (
                pc_hal::traits::DsMapFlags::CACHING_MASK,
                l4re::mem::DsMapFlags::CACHING_MASK,
            ),
        ];

        let pairs_attach = [
            (
                pc_hal::traits::DsAttachFlags::SEARCH_ADDR,
                l4re::mem::DsAttachFlags::SEARCH_ADDR,
            ),
            (
                pc_hal::traits::DsAttachFlags::IN_AREA,
                l4re::mem::DsAttachFlags::IN_AREA,
            ),
            (
                pc_hal::traits::DsAttachFlags::EAGER_MAP,
                l4re::mem::DsAttachFlags::EAGER_MAP,
            ),
        ];

        let translated_alloc =
            pairs_alloc
                .iter()
                .fold(l4re::mem::MaFlags::empty(), |acc, (pc_flag, l4_flag)| {
                    if (alloc_flags.bits() & pc_flag.bits()) != 0 {
                        acc | *l4_flag
                    } else {
                        acc
                    }
                });

        let translated_map =
            pairs_map
                .iter()
                .fold(l4re::mem::DsMapFlags::empty(), |acc, (pc_flag, l4_flag)| {
                    if (map_flags.bits() & pc_flag.bits()) != 0 {
                        acc | *l4_flag
                    } else {
                        acc
                    }
                });

        let translated_attach = pairs_attach.iter().fold(
            l4re::mem::DsAttachFlags::empty(),
            |acc, (pc_flag, l4_flag)| {
                if (attach_flags.bits() & pc_flag.bits()) != 0 {
                    acc | *l4_flag
                } else {
                    acc
                }
            },
        );

        let mem = l4re::mem::Dataspace::alloc(size, translated_alloc)?;
        Ok(MappableMemory(l4re::mem::Dataspace::attach(
            mem,
            translated_map,
            translated_attach,
        )?))
    }
    fn map_dma(&mut self, target: &Self::DmaSpace) -> Result<usize, Self::Error> {
        self.0.map_dma(&target.0)
    }
}
