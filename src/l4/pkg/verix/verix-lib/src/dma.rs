// TODO: maybe this fits into pc-hal-utils we will see

use std::collections::VecDeque;
use std::fmt;
use std::fmt::Debug;
use std::ops::Deref;
use std::ops::DerefMut;
use std::slice;
use std::{cell::RefCell, mem};

use log::trace;
use pc_hal::traits::RawMemoryInterface;
use pc_hal::traits::{DsAttachFlags, DsMapFlags, MaFlags, MemoryInterface};

use crate::types::{RxQueue, TxQueue};

pub struct DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub mem: MM,
    pub device_addr: usize,
    pub size: usize
}

pub struct Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub num_entries: usize,
    pub entry_size: usize,
    pub shared: RefCell<SharedPart<E, Dma, MM>>,
}

pub struct SharedPart<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub mem: DmaMemory<E, Dma, MM>,
    pub free_stack: Vec<usize>,
}

pub struct Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    addr_virt: *mut u8,
    addr_phys: usize,
    len: usize,
    pool: &'a Mempool<E, Dma, MM>,
    pool_entry: usize,
}

impl<E, Dma, MM> DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn new(size: usize, space: &Dma, super_pages: bool) -> Result<Self, E> {
        trace!("Allocating {} bytes of memory for DMA", size);
        let mut maflags = MaFlags::PINNED | MaFlags::CONTINUOUS;
        if super_pages {
            maflags |= MaFlags::SUPER_PAGES;
        }
        let mut mem = MM::alloc(
            size,
            maflags,
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

        Ok(Self { mem, device_addr, size })
    }

    // TODO: we do not necessarily have to memset everything, just the mapped chunk...
    pub fn memset(&mut self, val: u8) {
        for i in 0..self.size {
            unsafe { self.write8(i, val) }
        }
    }

    pub fn size(&self) -> usize {
        self.size
    }

    pub fn device_addr(&self) -> usize {
        self.device_addr
    }
}

impl<E, Dma, MM> RawMemoryInterface for DmaMemory<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    #[inline(always)]
    fn ptr(&self) -> *mut u8 {
        self.mem.ptr()
    }
}

impl<E, Dma, MM> Mempool<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn new(num_entries: usize, entry_size: usize, space: &Dma) -> Result<Self, E> {
        trace!(
            "Setting up mempool with {} entries, of size {} bytes",
            num_entries,
            entry_size
        );
        // TODO: ixy allows the OS to be non contigious here, if i figure out how to tell L4 to translate arbitrary addresses we can do that
        let mut mem: DmaMemory<E, Dma, MM> = DmaMemory::new(num_entries * entry_size, space, true)?;

        // Clear the memory to a defined initial state
        mem.memset(0x0);

        // Initially all elements are free
        let mut free_stack = Vec::with_capacity(num_entries);
        free_stack.extend(0..num_entries);

        let pool = Self {
            num_entries,
            entry_size,
            shared: RefCell::new(SharedPart { mem, free_stack }),
        };
        trace!("Mempool setup done");

        Ok(pool)
    }

    pub fn alloc_buf(&self) -> Option<usize> {
        self.shared.borrow_mut().free_stack.pop()
    }

    pub(crate) fn free_buf(&self, id: usize) {
        self.shared.borrow_mut().free_stack.push(id);
    }

    pub(crate) fn free_chunk<I>(&self, i: I)
    where
        I: Iterator<Item = usize>,
    {
        self.shared.borrow_mut().free_stack.extend(i);
    }

    pub fn get_device_addr(&self, entry: usize) -> usize {
        self.shared.borrow().mem.device_addr + entry * self.entry_size
    }

    pub fn get_our_addr(&self, entry: usize) -> *mut u8 {
        unsafe {
            self.shared
                .borrow_mut()
                .mem
                .ptr()
                .add(entry * self.entry_size)
        }
    }

    pub fn contains_device_addr<T>(&self, ptr: *mut T) -> bool {
        let ptr = ptr as usize;
        let device_base = self.shared.borrow().mem.device_addr; 
        (ptr >= device_base) && (ptr <= device_base + self.entry_size * self.num_entries)
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

impl<'a, E, Dma, MM> Deref for Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        unsafe { slice::from_raw_parts(self.addr_virt, self.len) }
    }
}

impl<'a, E, Dma, MM> DerefMut for Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn deref_mut(&mut self) -> &mut [u8] {
        unsafe { slice::from_raw_parts_mut(self.addr_virt, self.len) }
    }
}

impl<'a, E, Dma, MM> Debug for Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        (**self).fmt(f)
    }
}

impl<'a, E, Dma, MM> Drop for Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    fn drop(&mut self) {
        self.pool.free_buf(self.pool_entry);
    }
}

impl<'a, E, Dma, MM> Packet<'a, E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub fn new(pool: &'a Mempool<E, Dma, MM>, buf: usize, len: usize) -> Self {
        Self {
            addr_virt: pool.get_our_addr(buf),
            addr_phys: pool.get_device_addr(buf),
            len,
            pool: pool.clone(),
            pool_entry: buf,
        }
    }

    pub fn get_device_addr(&mut self) -> usize {
        self.addr_phys
    }

    pub fn get_pool_entry(&mut self) -> usize {
        self.pool_entry
    }
}

pub fn alloc_pkt_batch<'a, E, Dma, MM>(
    pool: &'a Mempool<E, Dma, MM>,
    buffer: &mut VecDeque<Packet<'a, E, Dma, MM>>,
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

pub fn alloc_pkt<'a, E, Dma, MM>(
    pool: &'a Mempool<E, Dma, MM>,
    size: usize,
) -> Option<Packet<'a, E, Dma, MM>>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    if size > pool.entry_size {
        return None;
    }

    pool.alloc_buf().map(|id| Packet {
        addr_virt: pool.get_our_addr(id),
        addr_phys: pool.get_device_addr(id),
        len: size,
        pool: &pool,
        pool_entry: id,
    })
}
