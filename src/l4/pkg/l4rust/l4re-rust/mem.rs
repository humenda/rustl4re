// ToDo: proc macros didn't support doc comments at some point, make them support doc strings and
// reformat the two interfaces
use l4::{
    cap::Cap,
    utcb::SndFlexPage as FlexPage,
    sys::{l4_addr_t, l4_size_t, PROTO_EMPTY},
};
use l4_derive::{iface, l4_client};
use super::sys::L4ReProtocols::L4RE_PROTO_DATASPACE;

const PROTO_DATASPACE: i64 = L4RE_PROTO_DATASPACE as i64;

/// Information about the dataspace
#[repr(C)]
#[derive(Clone)]
pub struct DsStats {
    /// size of dataspace
    pub size: u64,
    /// dataspace flags
    pub flags: u64,
}

// the struct is #[repr(C)] and hence the default implementation can be used
unsafe impl l4::ipc::Serialisable for DsStats { }
unsafe impl l4::ipc::Serialiser for DsStats {
    #[inline]
    unsafe fn read(mr: &mut l4::utcb::UtcbMr, _: &mut l4::ipc::BufferAccess) -> l4::error::Result<Self> {
        mr.read::<Self>() // most types are read from here
    }
    #[inline]
    unsafe fn write(self, mr: &mut l4::utcb::UtcbMr) -> l4::error::Result<()> {
        mr.write::<Self>(self)
    }
}

#[l4_client(DataspaceProvider)]
pub struct Dataspace;

/// used as return type for the **deprecated** phys() method of the DataspaceProvider trait
#[derive(Clone)]
#[repr(C)]
pub struct DeprecatedPhys {
    phys_addr: l4_addr_t,
    phys_size: l4_size_t
}
unsafe impl l4::ipc::Serialisable for DeprecatedPhys { }
unsafe impl l4::ipc::Serialiser for DeprecatedPhys {
    #[inline]
    unsafe fn read(mr: &mut l4::utcb::UtcbMr, _: &mut l4::ipc::BufferAccess) -> l4::error::Result<Self> {
        mr.read::<Self>()
    }
    #[inline]
    unsafe fn write(self, mr: &mut l4::utcb::UtcbMr) -> l4::error::Result<()> {
        mr.write::<Self>(self)
    }
}

// ToDo: this interface is not functional, the map operation is **broken**; it serves mostly as a
// marker trait and needs to be fixed; only the info() operation is tested
iface! {
    trait DataspaceProvider {
        const PROTOCOL_ID: i64 = PROTO_DATASPACE;
        // ToDo: docs
        fn map(&mut self, offset: l4_addr_t, spot: l4_addr_t, flags: u64,
                r: FlexPage) -> Cap<Dataspace>;

        /// Clear parts of a dataspace.
        ///
        /// Clear region within dataspace. `offset` gives the offset within the
        /// dataspace to mark the beginning and `offset + size` the end of the region.
        /// The memory is either zeroed out or unmapped and replaced through a null
        /// page mapping.
        fn clear(&mut self, offset: usize, size: usize) -> ();

        /// Get information on the dataspace.
        fn info(&mut self) -> DsStats;

        /// Copy contents from another dataspace.
        ///
        /// The copy operation may use copy-on-write mechanisms. The operation may also fail if both
        /// dataspaces are not from the same dataspace manager or the dataspace managers do not
        /// cooperate.
        fn copy_in(&mut self, dst_offset: l4_addr_t, src: Cap<Dataspace>,
                src_offset: l4_addr_t, size: l4_addr_t) -> ();

        /// Take operation.
        /// **Deprecated**: Dataspaces exists as long as a capability on the dataspace exists.
        fn take(&mut self, todo_delme: i32) -> ();

        /// Release operation.
        /// **Deprecated:** Dataspaces exist as long as a capability on the dataspace exists.
        fn release(&mut self, todo_delme: i32) -> ();

        /// Get the physical addresses of a dataspace.
        ///
        /// This call will only succeed on pinned memory dataspaces.  
        /// Returns phys_size
        /// **Deprecated:** Use L4Re::Dma_space instead. This function will be removed
        fn phys(&mut self, offset: l4_addr_t) -> DeprecatedPhys;

        /// Allocate a range in the dataspace.
        ///
        /// On success, at least the given range is guaranteed to be allocated. The
        /// dataspace manager may also allocate more memory due to page granularity.
        ///
        /// The memory is allocated with the same rights as the dataspace
        /// capability.
        ///
        /// A dataspace provider may also silently ignore areas
        /// outside the dataspace.
        fn allocate(&mut self, offset: l4_addr_t, size: l4_size_t) -> ();
    }
}

/// Flags for memory allocation
#[repr(u8)]
pub enum MemAllocFlags {
    /// Allocate physically contiguous memory
    Continuous   = 0x01,
    /// Allocate super pages
    SuperPages  = 0x04,
}

/// Memory allocation interface.
///
/// The memory-allocator API is the basic API to allocate memory from the L4Re
/// subsystem. The memory is allocated in terms of dataspaces (see
/// [l4re::mem::dataspace](trait.Dataspace.html).
/// The provided dataspaces have at least the property that data written to such
/// a dataspace is available as long as the dataspace is not freed or the data
/// is not overwritten. In particular, the memory backing a dataspace from an
/// allocator need not be allocated instantly, but may be allocated lazily on
/// demand. 
///
/// A memory allocator can provide dataspaces with additional properties, such
/// as physically contiguous memory, pre-allocated memory, or pinned memory. To
/// request memory with an additional property the `alloc()` method provides a
/// flags parameter. If the concrete implementation of a memory allocator does
/// not support or allow allocation of memory with a certain property, the
/// allocation may be refused.
///
/// Memory is not freed using this interface; use
/// `l4_task_unmap(mem, L4_FP_DELETE_OBJ)` or other similar means to remove a
/// dataspace.
iface! {
    trait MemoryAllocator {
        const PROTOCOL_ID: i64 = PROTO_EMPTY;
        /// Allocate anonymous memory.
        ///
        /// The size is given in bytes and has the granularity of a (super)page.
        /// If `size` is a negative value and `flags` set the
        /// [MemAllocFlags::Continuous](enum.MemAllocFlags.html) bit the
        /// allocator tries to allocate as much memory as possible leaving an
        /// amount of at least `-size` bytes within the associated quota.
        /// The flags parameters controls the allocation, see [MemAllocFlags](enum.MemAllocFlags.html)
        /// If `align` is given and the allocator supports it, the memory will
        /// be aligned and will be at least L4_PAGESHIFT, with SuperPages flag
        /// set at least L4_SUPERPAGESHIFT
        fn alloc(&mut self, size: isize, flags: u64, align: Option<u64>) -> Cap<Dataspace>;
    }
}

#[l4_client(MemoryAllocator, demand = 1)]
pub struct MemAlloc;
