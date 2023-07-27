// TODO: maybe this fits into pc-hal-utils we will see

use std::{cell::RefCell, rc::Rc};

use log::trace;
use pc_hal::traits::{MaFlags, DsMapFlags, DsAttachFlags, MemoryInterface32};

pub struct DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    mem: MM,
    device_addr: usize
}

pub struct Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    num_entries: usize,
    entry_size: usize,
    mem: DmaMemory<E, Dma, MM>,
    free_stack: RefCell<Vec<usize>>
}

impl<E, Dma, MM> DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    pub fn new(size: usize, space: &Dma) -> Result<Self, E>
    {

        trace!("Allocating 0x{:x} bytes of memory for DMA", size);
        let mut mem = MM::alloc(
            size,
            MaFlags::PINNED | MaFlags::CONTINUOUS,
            DsMapFlags::RW,
            DsAttachFlags::SEARCH_ADDR | DsAttachFlags::EAGER_MAP
        )?;

        trace!("Mapping memory to DMA");
        let device_addr = mem.map_dma(space)?;
        trace!("Mapped to device address space at 0x{:x}", device_addr);

        Ok(Self{
            mem,
            device_addr
        })
    }

    pub fn device_addr(&self) -> usize {
        self.device_addr
    }
}

impl<E, Dma, MM> MemoryInterface32 for DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    type Addr = usize;

    fn write8(&mut self, offset: Self::Addr, val: u8) {
        self.mem.write8(offset, val);
    }

    fn write16(&mut self, offset: Self::Addr, val: u16) {
        self.mem.write16(offset, val);
    }

    fn write32(&mut self, offset: Self::Addr, val: u32) {
        self.mem.write32(offset, val);
    }

    fn read8(&self, offset: Self::Addr) -> u8 {
        self.mem.read8(offset)
    }

    fn read16(&self, offset: Self::Addr) -> u16 {
        self.mem.read16(offset)
    }

    fn read32(&self, offset: Self::Addr) -> u32 {
        self.mem.read32(offset)
    }
}

impl<E, Dma, MM> Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    pub fn new(num_entries: usize, entry_size: usize, space: &Dma) -> Result<Rc<Self>, E>
    {
        // TODO: ixy allows the OS to be non contigious here, if i figure out how to tell L4 to translate arbitrary addresses we can do that
        let mut mem: DmaMemory<E, Dma, MM> = DmaMemory::new(num_entries * entry_size, space)?;

        // Clear the memory to a defined initial state
        for i in 0..(num_entries * entry_size) {
            mem.write8(i, 0x0);
        }

        // Initially all elements are free
        let mut free_stack = Vec::with_capacity(num_entries);
        free_stack.extend(0..num_entries);

        let pool = Self {
            num_entries,
            entry_size,
            mem,
            free_stack: RefCell::new(free_stack)
        };

        Ok(Rc::new(pool))
    }

    pub fn size(&self) -> usize {
        self.num_entries * self.entry_size
    }

    pub fn entry_size(&self) -> usize {
        self.entry_size
    }
}