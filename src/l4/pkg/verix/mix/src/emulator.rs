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
    pub (crate) resources: Vec<Resource>,
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
    alloc_flags: pc_hal::traits::MaFlags,
    map_flags: pc_hal::traits::DsMapFlags,
    attach_flags: pc_hal::traits::DsAttachFlags,
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
        // TODO: this is nice behavior because it effectively singletons
        // the devices, however it is not the behavior of the L4 API right
        // now (but it should be), make it so.
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
    pub fn new(ifaces: Vec<pc_hal::traits::BusInterface>, resources: Vec<Resource>) -> Self {
        Self { ifaces, resources }
    }
}

impl pc_hal::traits::Device for BasicDevice {
    type Resource = Resource;

    type ResourceIter<'a> = std::vec::IntoIter<Resource>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        self.resources.clone().into_iter()
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
    fn ptr(&mut self) -> *mut u8 {
        self.data.as_mut_ptr()
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
        alloc_flags: pc_hal::traits::MaFlags,
        map_flags: pc_hal::traits::DsMapFlags,
        attach_flags: pc_hal::traits::DsAttachFlags,
    ) -> Result<Self, Self::Error> {
        Ok(MappableMemory {
            data: vec![0; size],
            alloc_flags,
            map_flags,
            attach_flags,
        })
    }

    fn map_dma(&mut self, target: &Self::DmaSpace) -> Result<usize, Self::Error> {
        assert!(target.domain.is_some());
        let dom = target.domain.as_ref().unwrap();
        dom.map_dma(self.ptr(), self.data.len());
        Ok(self.ptr() as usize)
    }
}

/*
The TODO at the mappable memory and the iomem give me the capability to check whether
a) the io mappable memory that is getting accessed is actually correct, this is enforced by construction of the io mem mapping and
   doesn't need to be rechecked on runtime (modulo out of bounds)
b) If the device is told to access memory through DMA (RX/TX desc + Packet buffer) it can check if it is allowed to access that memory
   and potentially cause a "DMA fault" (in form of a panic)

What is left after this is the implementation of the actual device. This comes through three interfaces:
1. the PCI config space:
    - For the config space the driver already explicitly yields to me and does *not* access it through pointers (as it is done via vbus in L4).
      This makes it the easiest target for experimentation for now
    - The interesting part here is: How do I nicely implement an MMIO emulator. I think I might be able to steal some design from:
        - qemu
        - rust based emulators?
        - potentially ocaml or haskell based emulators?
2. the PCI BARs/MMIO. These pose an additional challenge, I need to make it such that:
    - the driver always yields to me despite accessing it straight through pointers. This should be doable by inserting yield points into auto
      generated API.
    - For additional safety i could keep a shadow state that gets kept in sync with the external one accessible through pointers and assert they are
      in sync. However I am uncertain about how this performs in the model checker so this is something to check
    - This should share the actual register abstraction with the PCI part if possible.
3. DMA: This in turn is easier again. Here I merely need to
    - assert if the DMA memory i am trying to access is mapped for me and that it is in bounds
    - has the correct permissions (and potentially other flags, I'll have to experimentally check how much of what I do rn is necessary)
    - this API needs no further considertations I think, I can just read/write to it with proper preconditions

This leaves me with the main issue if designing a good register spec. I believe it should:
- potentially make use of macros too
- Delegate writes/reads to specific functions for registers based on a dispatch function that checks addresses.
  - in particular we want to be able to panic for non implemented sections
  - in particular we want to protect registers that are not readable/writeable
- these specific functions should get access to the current device state and update it if necessary.
  - in particular they should be able to panic if necessary
- The device state cannot be completly reflected in the memory map since there are certain fields that get set to 1 and then polled until set to 1 etc. This means we want to
  keep at least some internal state. Potentially fully seperate from the memory map. This also throws up the question if we want there to
  be a bijective function between the two? This would allow us to recover a memory map from an internal state
 */
