// TODO: maybe this fits into pc-hal-utils we will see

use std::collections::VecDeque;
use std::fmt;
use std::fmt::Debug;
use std::ops::Deref;
use std::ops::DerefMut;
use std::slice;
use std::{cell::RefCell, mem, rc::Rc};

use log::trace;
use pc_hal::traits::{DsAttachFlags, DsMapFlags, MaFlags, MemoryInterface};

use crate::types::{RxQueue, TxQueue};

pub const PACKET_HEADROOM: usize = 32;

pub struct DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    mem: MM,
    device_addr: usize,
}

pub struct Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    num_entries: usize,
    entry_size: usize,
    pub(crate) mem: RefCell<DmaMemory<E, Dma, MM>>,
    pub(crate) free_stack: RefCell<Vec<usize>>,
}

pub struct Packet<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub(crate) addr_virt: *mut u8,
    pub(crate) addr_phys: usize,
    pub(crate) len: usize,
    pub(crate) pool: Rc<Mempool<E, Dma, MM>>,
    pub(crate) pool_entry: usize,
}

impl<E, Dma, MM> DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn new(size: usize, space: &Dma) -> Result<Self, E> {
        trace!("Allocating {} bytes of memory for DMA", size);
        let mut mem = MM::alloc(
            size,
            MaFlags::PINNED | MaFlags::CONTINUOUS,
            DsMapFlags::RW,
            DsAttachFlags::SEARCH_ADDR | DsAttachFlags::EAGER_MAP,
        )?;

        trace!("Mapping memory to DMA");
        let device_addr = mem.map_dma(space)?;
        trace!(
            "Allocated {} bytes, our addr {:p}, device addr 0x{:x}",
            size,
            mem.ptr(),
            device_addr
        );

        Ok(Self { mem, device_addr })
    }

    pub fn find_addr_of_non_zero(&mut self, len: usize) -> Option<usize> {
        let base_ptr = self.ptr();
        for offset in 0..len {
            let (curr_ptr, val) = unsafe {
                let curr_ptr = base_ptr.add(offset);
                (curr_ptr, core::ptr::read(curr_ptr))
            };
            if val != 0 {
                return Some(curr_ptr as usize);
            }
        }
        return None;
    }

    pub fn device_addr(&self) -> usize {
        self.device_addr
    }
}

impl<E, Dma, MM> MemoryInterface for DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn ptr(&mut self) -> *mut u8 {
        self.mem.ptr()
    }
}

impl<E, Dma, MM> Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn new(num_entries: usize, entry_size: usize, space: &Dma) -> Result<Rc<Self>, E> {
        trace!(
            "Setting up mempool with {} entries, of size {} bytes",
            num_entries,
            entry_size
        );
        // TODO: ixy allows the OS to be non contigious here, if i figure out how to tell L4 to translate arbitrary addresses we can do that
        let mut mem: DmaMemory<E, Dma, MM> = DmaMemory::new(num_entries * entry_size, space)?;

        // Clear the memory to a defined initial state
        for i in 0..(num_entries * entry_size) {
            unsafe { core::ptr::write_volatile(mem.ptr().add(i), 0x61) }
        }

        // Initially all elements are free
        let mut free_stack = Vec::with_capacity(num_entries);
        free_stack.extend(0..num_entries);

        let pool = Self {
            num_entries,
            entry_size,
            mem: RefCell::new(mem),
            free_stack: RefCell::new(free_stack),
        };
        trace!("Mempool setup done");

        Ok(Rc::new(pool))
    }

    pub fn alloc_buf<'a>(&'a self) -> Option<usize> {
        self.free_stack.borrow_mut().pop()
    }

    pub(crate) fn free_buf(&self, id: usize) {
        assert!(id < self.num_entries, "buffer outside of memory pool");

        self.free_stack.borrow_mut().push(id);
    }

    pub fn get_device_addr(&self, entry: usize) -> usize {
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

// TODO: dedup
impl<E, Dma, MM> RxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn nth_descriptor_ptr(&mut self, nth: usize) -> *mut u8 {
        unsafe { self.descriptors.ptr().add(mem::size_of::<[u64; 2]>() * nth) }
    }
}

impl<E, Dma, MM> TxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn nth_descriptor_ptr(&mut self, nth: usize) -> *mut u8 {
        unsafe { self.descriptors.ptr().add(mem::size_of::<[u64; 2]>() * nth) }
    }
}

impl<E, Dma, MM> Deref for Packet<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        unsafe { slice::from_raw_parts(self.addr_virt, self.len) }
    }
}

impl<E, Dma, MM> DerefMut for Packet<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn deref_mut(&mut self) -> &mut [u8] {
        unsafe { slice::from_raw_parts_mut(self.addr_virt, self.len) }
    }
}

impl<E, Dma, MM> Debug for Packet<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        (**self).fmt(f)
    }
}

impl<E, Dma, MM> Drop for Packet<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn drop(&mut self) {
        self.pool.free_buf(self.pool_entry);
    }
}

pub fn alloc_pkt_batch<E, Dma, MM>(
    pool: &Rc<Mempool<E, Dma, MM>>,
    buffer: &mut VecDeque<Packet<E, Dma, MM>>,
    num_packets: usize,
    packet_size: usize,
) -> usize
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    let mut allocated = 0;

    while let Some(p) = alloc_pkt(pool, packet_size) {
        buffer.push_back(p);

        allocated += 1;
        if allocated >= num_packets {
            break;
        }
    }

    allocated
}

pub fn alloc_pkt<E, Dma, MM>(
    pool: &Rc<Mempool<E, Dma, MM>>,
    size: usize,
) -> Option<Packet<E, Dma, MM>>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    if size > pool.entry_size - PACKET_HEADROOM {
        return None;
    }

    pool.alloc_buf().map(|id| Packet {
        addr_virt: unsafe { pool.get_our_addr(id).add(PACKET_HEADROOM) },
        addr_phys: pool.get_device_addr(id) + PACKET_HEADROOM,
        len: size,
        pool: Rc::clone(pool),
        pool_entry: id,
    })
}
