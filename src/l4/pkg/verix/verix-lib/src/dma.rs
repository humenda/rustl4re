// TODO: maybe this fits into pc-hal-utils we will see

use std::{cell::RefCell, rc::Rc, mem};

use log::trace;
use pc_hal::traits::{MaFlags, DsMapFlags, DsAttachFlags, MemoryInterface};

use crate::types::RxQueue;

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
    mem: RefCell<DmaMemory<E, Dma, MM>>,
    free_stack: RefCell<Vec<usize>>
}

impl<E, Dma, MM> DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    pub fn new(size: usize, space: &Dma) -> Result<Self, E>
    {

        trace!("Allocating {} bytes of memory for DMA", size);
        let mut mem = MM::alloc(
            size,
            MaFlags::PINNED | MaFlags::CONTINUOUS,
            DsMapFlags::RW,
            DsAttachFlags::SEARCH_ADDR | DsAttachFlags::EAGER_MAP
        )?;

        trace!("Mapping memory to DMA");
        let device_addr = mem.map_dma(space)?;

        Ok(Self{
            mem,
            device_addr
        })
    }

    pub fn device_addr(&self) -> usize {
        self.device_addr
    }
}

impl<E, Dma, MM> MemoryInterface for DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace
{
    fn ptr(&mut self) -> *mut u8 {
        self.mem.ptr()
    }
}

impl<E, Dma, MM> Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>, 
    Dma: pc_hal::traits::DmaSpace
{
    pub fn new(num_entries: usize, entry_size: usize, space: &Dma) -> Result<Rc<Self>, E>
    {
        trace!("Setting up mempool with {} entries, of size {} bytes", num_entries, entry_size);
        // TODO: ixy allows the OS to be non contigious here, if i figure out how to tell L4 to translate arbitrary addresses we can do that
        let mut mem: DmaMemory<E, Dma, MM> = DmaMemory::new(num_entries * entry_size, space)?;

        // Clear the memory to a defined initial state
        for i in 0..(num_entries * entry_size) {
            unsafe { core::ptr::write_volatile(mem.ptr().add(i), 0x0)}
        }

        // Initially all elements are free
        let mut free_stack = Vec::with_capacity(num_entries);
        free_stack.extend(0..num_entries);

        let pool = Self {
            num_entries,
            entry_size,
            mem: RefCell::new(mem),
            free_stack: RefCell::new(free_stack)
        };
        trace!("Mempool setup done");

        Ok(Rc::new(pool))
    }

    pub fn alloc_buf<'a>(&'a self) -> Option<usize> {
        let idx = self.free_stack.borrow_mut().pop()?;
        Some(idx)
    }

    pub fn get_device_addr(&self, entry: usize) -> usize
    {
        self.mem.borrow().device_addr + entry * self.entry_size
    }

    pub fn get_our_addr(&self, entry: usize) -> *mut u8 {
        unsafe { self.mem.borrow_mut().ptr().add(entry * self.entry_size) }
    }

    pub fn size(&self) -> usize {
        self.num_entries * self.entry_size
    }

    pub fn entry_size(&self) -> usize {
        self.entry_size
    }
}

impl<E, Dma, MM> RxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn nth_descriptor_ptr(&mut self, nth: usize) -> *mut u8 {
        unsafe { self.descriptors.ptr().add(mem::size_of::<[u64; 2]>() * nth) }
    }
}