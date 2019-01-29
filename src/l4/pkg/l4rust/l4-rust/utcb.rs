use _core::{intrinsics::transmute,
    marker::PhantomData,
    mem::{align_of, size_of},
    ops::{Deref, DerefMut}
};

use l4_sys::{
    l4_buf_regs_t,
    l4_msg_item_consts_t::*,
    L4_fpage_rights::*,
    L4_fpage_type::*,
    l4_utcb, l4_utcb_br, l4_utcb_mr_u, l4_utcb_t, l4_msg_regs_t,
    consts::{UTCB_GENERIC_DATA_SIZE, UtcbConsts}};

use crate::cap::{Cap, Interface};
use crate::ipc::Serialisable;
use crate::error::{Error, GenericErr, Result};
use crate::types::{Mword, UMword};

/// UTCB constants (architecture-specific)
pub use crate::l4_sys::consts::UtcbConsts as Consts;

/// Number of words used by an item when written to the message registers
pub const WORDS_PER_ITEM: usize = 2;

#[macro_export]
macro_rules! utcb_mr {
    ($utcb:ident <- $($arg:expr),*) => (
        $(println!("{}", $arg + $arg);)*
    )
}


// see union l4_msg_regs_t
const MSG_REG_COUNT: usize = UTCB_GENERIC_DATA_SIZE / 
        (size_of::<UMword>() / size_of::<UMword>());


// NOTE: this may never be `Send`!
/// User space Thread Control Block
///
/// The UTCB is used for storing data for IPC operations and furthermore provides means for the
/// user to interact with a thread instance. More details are to be found in the official L4Re
/// documentation.
///
/// # Example
///
/// ```
/// use l4::Utcb;
///
/// fn main() {
///     let u = Utcb()::current(); // UTCB of current thread
///     u.mr_mut()[0] = 20;
/// }
pub struct Utcb {
    pub raw: *mut l4_utcb_t, // opaque UTCB struct
}

impl Utcb {
    pub fn from_utcb(utcb: *mut l4_utcb_t) -> Utcb {
        Utcb { raw: utcb }
    }

    pub fn current() -> Utcb {
        unsafe { // this can't fail: no UTCB, no thread -> no execution
            Utcb { raw: l4_utcb() }
        }
    }

    #[inline]
    pub fn mr_raw(&self) -> &[u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &*(ptr as *const [u64; MSG_REG_COUNT])
        }
    }

    #[inline]
    pub fn mr_raw_mut(&mut self) -> &mut [u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &mut *(ptr as *mut [u64; MSG_REG_COUNT])
        }
    }

    /// Create message register builder
    ///
    /// This returns a message builder for the message registers. Unlike the raw
    /// C version, this builder is capable to write byte-granular to the message
    /// registers, allowing both space and performance-optimisations due to
    /// alignment. See [UtcbMr](struct.UtcbMr.html) for more details.
    #[inline]
    pub fn mr(&mut self) -> UtcbMr {
        unsafe {
            UtcbMr::from_utcb(self.raw)
        }
    }

    /// Create buffer register builder
    ///
    /// This returns a builder for the buffer registers. Unlike the raw
    /// C version, this builder is capable to write byte-granular to the message
    /// registers, allowing both space and performance-optimisations due to
    /// alignment. See [UtcbBr](struct.UtcbBr.html) for more details.
    #[inline]
    pub fn br(&mut self) -> UtcbBr {
        unsafe {
            UtcbBr::from_utcb(self.raw)
        }
    }

    #[inline(always)]
    pub fn bdr(&self) -> usize {
        unsafe {
            (*l4_utcb_br()).bdr as usize
        }
    }
}
/// Flex page types
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum FpageType {
    Special = (L4_FPAGE_SPECIAL as u8) << 4,
    Memory  = (L4_FPAGE_MEMORY as u8) << 4,
    Io      = (L4_FPAGE_IO as u8) << 4,
    Obj     = (L4_FPAGE_OBJ as u8) << 4
}

/// capability mapping type (flex page type)
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum MapType {
    Map   = L4_MAP_ITEM_MAP as u8,
    Grant = L4_MAP_ITEM_GRANT as u8,
}

bitflags! {
    pub struct FpageRights: u8 {
        /// executable flex page
        const X   = L4_FPAGE_X as u8;
        /// writable flex page
        const W   = L4_FPAGE_W as u8;
        /// read-only flex page
        const R   = L4_FPAGE_RO as u8;
        /// flex page which is readable and writable
        const RW  = Self::W.bits | Self::R.bits;
        /// flex page which is readable and executable
        const RX  = Self::R.bits | Self::X.bits;
        /// flex page which is readable, executable and writable
        const RWX = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}

/// A flex page is the representation of memory, I/O ports or kernel objects (capabilities)
/// transferable across task boundaries. Consequently, it carries additional information as access
/// rights and more.
/// See the [C API](https://l4re.org/doc/group__l4__fpage__api.html) for a more in-depth
/// discussion.
/// Sending a capability requires to registers, one containing the action on the
/// page, the other the flexpage to be transfered.
#[repr(C)]
#[derive(Clone)]
pub struct SndFlexPage {
    /// information about base address to send, map mode, etc.
    description: u64,
    // the data to transfer, e.g. a capability
    fpage: u64
}

impl SndFlexPage {
    /// Construct a new flex page
    ///
    /// This constructs a new flex page from the raw value of a raw flex page
    /// (`l4_fpage_t.raw`) and additional information.
    pub fn new(fpage: u64, snd_base: u64, mt: Option<MapType>) -> Self {
        // this is taken mostly unmodified from the Gen_Fpage class in `ipc_types`, though cache
        // opts and `continue` (enums from this class) are not supported
        Self {
            // AKA _base in the C++ version
            description: L4_ITEM_MAP as u64
                    | (snd_base & (!0u64 << 10))
                    | mt.map(|v| v as u64).unwrap_or_default(),
            fpage: fpage
        }
    }

    pub fn from_cap<T: Interface>(c: &Cap<T>, rights: FpageRights,
                mt: Option<MapType>) -> SndFlexPage {
        SndFlexPage::new(unsafe {
                ::l4_sys::l4_obj_fpage(c.cap(), 0, rights.bits() as u8).raw
            }, 0, mt)
    }
}

/// Return an aligned pointer to the next free message register
///
/// The pointer is advanced by the end of the already written message registers,
/// aligned to the required type. Returned is a
/// pointer to the type-aligned memory region.
#[inline]
pub unsafe fn align_with_offset<T>(ptr: *mut u8, offset: usize)
        -> (*mut u8, usize) {
    let ptr = ptr.offset(offset as isize);
    let new_offset = ptr.align_offset(align_of::<T>());
    (ptr.offset(new_offset as isize), offset + new_offset)
}

// make the dumb C types more smart: let them know their limits
pub trait UtcbRegSize {
    // size in bytes
    const BUF_SIZE: usize;
}

impl UtcbRegSize for l4_msg_regs_t {
    const BUF_SIZE: usize = UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize
            * size_of::<UMword>();
}

impl UtcbRegSize for l4_buf_regs_t {
    const BUF_SIZE: usize = UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE as usize
            * size_of::<UMword>();
}


/// Convenience interface for message serialisation to the message registers
///
/// This thin type wraps the pointer to the message reigsters,  offering type-aligned,
/// byte-granular write and read access. It also does offset accounting, so that data only needs to
/// be stuffed in or dragged out. It is not meant to be used directly, but through the more
/// convenient and safe [UtcbMr](struct.UtcbMr.html) and [UtcbBr](struct.UtcbBr.html) types.
pub struct Registers<B: UtcbRegSize> {
    buf: *mut u8, // casted u64 pointer to msg or buf registers
    offset: usize,
    size: PhantomData<B>,
}

impl<U: UtcbRegSize> Registers<U> {
    /// Initialise struct from given pointer
    ///
    /// The pointer must be valid.
    pub unsafe fn from_raw(buf: *mut u64) -> Self {
        Registers {
            buf: transmute::<*mut u64, *mut u8>(buf as *mut _),
            offset: 0,
            size: PhantomData,
        }
    }

    /// Write given value to the next free, type-aligned slot
    ///
    /// The value is aligned and written into the message registers.
    pub unsafe fn write<T: Serialisable>(&mut self, val: T) 
            -> Result<()> {
        let (ptr, offset) = align_with_offset::<T>(self.buf, self.offset);
        let next = offset + size_of::<T>();
        if next > U::BUF_SIZE {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        *transmute::<*mut u8, *mut T>(ptr) = val;
        self.offset = next; // advance offset *behind* element
        Ok(())
    }

    /// Read given type from the memory region at the internal buffer + offset
    ///
    /// This function reads from the given pointer + the internal offset a value
    /// from the specified type. If a read beyond the message registers is
    /// attempted, an error is returned.
    /// This is unsafe due to a missing guarantee that
    /// the pointer to the UTCB, with which the object was constructed is valid
    /// and that the memory region behind  has the length of
    /// `UTCB_DATA_SIZE_IN_BYTES`.
    pub unsafe fn read<T: Serialisable>(&mut self)  -> Result<T> {
        let (ptr, offset) = align_with_offset::<T>(self.buf, self.offset);
        let next = offset + size_of::<T>();
        if next > U::BUF_SIZE {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        self.offset = next;
        let val: T = (*transmute::<*mut u8, *mut T>(ptr)).clone();
        Ok(val)
    }

    /// Mwords written to this buffer (rounded up)
    #[inline]
    pub fn words(&self) -> u32 {
        (self.offset / size_of::<Mword>() +
            match self.offset % size_of::<usize>() {
                0 => 0,
                _ => 1
            }) as u32
    }
    /// Reset internal offset to beginning of message registers
    #[inline]
    pub fn reset(&mut self) {
        self.offset = 0
    }
}

/// (mostly) safe and convenient access to the message registers of the UTCB
///
/// This type offers byte-granular and type-aligned write and read access to the message registers
/// of the UTCB. It does book keeping so that data can be written / read without the need to care
/// about the exact position.
///
/// # Example
///
/// ```
/// use crate::l4::utcb::{SndFlexPage, Utcb};
///
/// fn main() {
///     let mr = Utcb::current()::mr();
///     mr.write(42u32);
///     mr.write(true); // trve
///     mr.write(SndFlexPage::new(0, 0, None));
///     // see the [Mword](../types/type.Mword.html) type, first two writes
///     // fit into one mword
///     assert_eq!(mr.words(), 3); // flex page takes two ewords
///     // items (kernel objects) such as flex pages are additionally counted
///     assert_eq!(mr.items(), 1);
/// }
/// ```
pub struct UtcbMr {
    regs: Registers<l4_msg_regs_t>,
    // number of items, used to subtract from the number of written words; flex pages take up two
    // Mwords, therefore number of flex pages (AKA items) needs to be stored
    items: u32,
}

impl UtcbMr {
    pub unsafe fn from_utcb(u: *mut l4_utcb_t) -> Self {
        Self::from_mr(l4_sys::l4_utcb_mr_u(u))
    }

    /// Write given item to the next free, type-aligned slot
    ///
    /// The item (e.g. a flex page) is aligned and written into the message registers.
    pub unsafe fn write_item<T: Serialisable>(&mut self, val: T) 
            -> Result<()> {
        let res = self.regs.write(val)?;
        self.items += 1;
        Ok(res)
    }

    /// Number of typed items in the UTCB
    #[inline]
    pub fn items(&self) -> u32 {
        self.items
    }

    /// Initialise `UtcbMr` from a raw `l4_msg_regs_t` pointer
    ///
    /// This is unsafe since the pointer must be valid.
    pub unsafe fn from_mr(m: *mut l4_msg_regs_t) -> UtcbMr {
        UtcbMr {
            regs: Registers::from_raw((*m).mr.as_mut().as_mut_ptr()),
            items: 0,
        }
    }

    /// Number of words in the UTCB
    ///
    /// Returns the number of **untyped** words of the UTCB. This is different
    /// from the number of **used** words in the message registers: the kernel
    /// distinguishes between untyped words and typed words (the latter are e.g.
    /// flexpages). To retrieve the number of used all words use
    /// `mr.words() + mr.items()`.
    pub fn words(&self) -> u32 {
        self.regs.words() - (self.items * 2) // subtract number of typed items
        //                   ^ items: 2 Mwords, kernel action and object
    }
}

impl Deref for UtcbMr {
    type Target = Registers<l4_msg_regs_t>;
    fn deref(&self) -> &Registers<l4_msg_regs_t> { &self.regs }
}

impl DerefMut for UtcbMr {
    fn deref_mut(&mut self) -> &mut Registers<l4_msg_regs_t> { &mut self.regs }
}

/// (almost) safe and convenient access to the buffer registers of the UTCB
///
/// This type offers byte-granular and type-aligned write and read access to the buffer registers
/// of the UTCB. It does book keeping so that data can be written / read without the need to care
/// about the exact position within the registers.
///
/// This type is safe to use as long as your thread still exists. If your thread
/// ceases to exist, the UTCB does as well and this object points to invalid
/// memory.  
/// This type does not protect the programmer from writing rubbish to the buffer
/// registers; if in doubt, read documentation on receive windows and UTCB
/// setup.
///
/// # Example
///
/// ```
/// use crate::l4::utcb::{SndFlexPage, Utcb};
///
/// fn main() {
///     let br = Utcb::current()::br();
///     ToDo: what to write
/// }
/// ```
pub struct UtcbBr(Registers<l4_buf_regs_t>);

impl UtcbBr {
    pub unsafe fn from_utcb(u: *mut l4_utcb_t) -> Self {
        Self::from_br(l4_sys::l4_utcb_br_u(u))
    }

    /// Number of typed items in the buffer registers
    pub fn words(&self) -> u32 {
        self.0.words() / 2 // each item takes up two words
    }

    /// Initialise `UtcbBr` from a raw `l4_buf_regs_t` pointer
    ///
    /// This is unsafe since the pointer must be valid.
    pub unsafe fn from_br(m: *mut l4_buf_regs_t) -> UtcbBr {
        UtcbBr(Registers::from_raw((*m).br.as_mut().as_mut_ptr()))
    }
}

impl Deref for UtcbBr {
    type Target = Registers<l4_buf_regs_t>;
    fn deref(&self) -> &Self::Target { &self.0 }
}

impl DerefMut for UtcbBr {
    fn deref_mut(&mut self) -> &mut Self::Target { &mut self.0 }
}


