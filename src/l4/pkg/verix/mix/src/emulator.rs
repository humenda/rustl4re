use std::cell::RefCell;
use std::mem;
use std::rc::Rc;

use pc_hal::prelude::*;
use pc_hal::traits::ResourceType;

use crate::ix::IxDevice;
/// General philosophy with respect to errors is:
/// since this is a model for a panic detecting
/// model checker like kani we always panic
/// instead of throwing errors. Throwing errors
/// that get ? causes unnecessary need for control
/// flow analysis.
#[derive(Debug)]
pub enum Error {}
pub struct Bus {
    devices: Vec<Device>,
    /// Contains starts of domains that are mapped
    mapped_domains: Vec<usize>,
}

pub struct BasicDevice {
    ifaces: Vec<pc_hal::traits::BusInterface>,
}

pub enum Device {
    Basic(BasicDevice),
    Ix(IxDevice),
}

#[derive(PartialEq, Eq, Clone)]
pub struct BasicResource {
    pub start: usize,
    pub end: usize,
    pub typ: pc_hal::traits::ResourceType,
}

#[derive(PartialEq, Eq, Clone)]
pub struct DmaDomainResource {
    pub start: usize,
    pub end: usize,
    /// (ptr, len)
    pub mapping: RefCell<Vec<(*mut u8, usize)>>,
}

#[derive(PartialEq, Eq, Clone)]
pub enum Resource {
    Basic(BasicResource),
    DmaDomain(Rc<DmaDomainResource>),
}

#[derive(PartialEq, Eq, Clone)]
pub struct DmaSpace {
    domain: Option<Rc<DmaDomainResource>>,
}
pub struct MappableMemory {
    data: Vec<u8>,
}

impl Bus {
    pub fn new(devices: Vec<Device>) -> Self {
        Self {
            devices,
            mapped_domains: Vec::new(),
        }
    }
}

impl pc_hal::traits::Bus for Bus {
    type Error = Error;
    type Device = Device;
    type Resource = Resource;
    type DmaSpace = DmaSpace;
    type DeviceIter = std::vec::IntoIter<Self::Device>;

    fn get() -> Option<Self> {
        // We don't implement this API since it is not used by verix-lib for now
        // consider getting rid of it since the Bus has to be supplied to verix-lib
        None
    }

    fn device_iter(&mut self) -> Self::DeviceIter {
        let data = mem::replace(&mut self.devices, vec![]);
        data.into_iter()
    }

    fn assign_dma_domain(
        &mut self,
        dma_domain: &mut Self::Resource,
        dma_space: &mut Self::DmaSpace,
    ) -> Result<(), Self::Error> {
        assert!(dma_domain.typ() == ResourceType::DmaDomain);
        let start = dma_domain.start();
        assert!(!self.mapped_domains.contains(&start));
        assert!(dma_space.domain.is_none());

        match dma_domain {
            Resource::DmaDomain(dom) => {
                self.mapped_domains.push(start);
                dma_space.domain = Some(dom.clone());
            }
            _ => unreachable!(),
        }
        Ok(())
    }
}

impl Device {
    fn basic(&self) -> &BasicDevice {
        match self {
            Device::Basic(base) => base,
            Device::Ix(ix) => &ix.basic,
        }
    }
}

impl BasicDevice {
    pub fn new(ifaces: Vec<pc_hal::traits::BusInterface>) -> Self {
        Self { ifaces }
    }
}

impl pc_hal::traits::Device for BasicDevice {
    type Resource = Resource;

    type ResourceIter<'a> = std::vec::IntoIter<Resource>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        vec![].into_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.ifaces.contains(&iface)
    }
}

impl pc_hal::traits::Device for Device {
    type Resource = Resource;

    type ResourceIter<'a> = std::vec::IntoIter<Resource>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        self.basic().resource_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.basic().supports_interface(iface)
    }
}

impl pc_hal::traits::Resource for Resource {
    fn start(&mut self) -> usize {
        match self {
            Resource::Basic(basic) => basic.start,
            Resource::DmaDomain(dom) => dom.start,
        }
    }

    fn end(&mut self) -> usize {
        match self {
            Resource::Basic(basic) => basic.end,
            Resource::DmaDomain(dom) => dom.end,
        }
    }

    fn typ(&self) -> pc_hal::traits::ResourceType {
        match self {
            Resource::Basic(basic) => basic.typ,
            Resource::DmaDomain(_) => pc_hal::traits::ResourceType::DmaDomain,
        }
    }
}

impl pc_hal::traits::DmaSpace for DmaSpace {
    fn new() -> Self {
        DmaSpace { domain: None }
    }
}

impl pc_hal::traits::RawMemoryInterface for MappableMemory {
    fn ptr(&self) -> *mut u8 {
        self.data.as_ptr() as *mut u8
    }
}

impl DmaDomainResource {
    fn map_dma(&self, ptr: *mut u8, size: usize) {
        let mut mapping = self.mapping.borrow_mut();
        mapping.push((ptr, size));
    }
}

impl pc_hal::traits::MappableMemory for MappableMemory {
    type Error = Error;
    type DmaSpace = DmaSpace;

    fn alloc(
        size: usize,
        _alloc_flags: pc_hal::traits::MaFlags,
        _map_flags: pc_hal::traits::DsMapFlags,
        _attach_flags: pc_hal::traits::DsAttachFlags,
    ) -> Result<Self, Self::Error> {
        Ok(MappableMemory {
            data: vec![0; size],
        })
    }

    fn map_dma(&mut self, target: &Self::DmaSpace) -> Result<usize, Self::Error> {
        assert!(target.domain.is_some());
        let dom = target.domain.as_ref().unwrap();
        dom.map_dma(self.ptr(), self.data.len());
        Ok(self.ptr() as usize)
    }
}
