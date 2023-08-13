use core::{
    convert::From,
    marker::PhantomData,
    mem::{align_of, size_of},
    ops::{Deref, DerefMut},
};

use l4_sys::{
    consts::{UtcbConsts, UTCB_GENERIC_DATA_SIZE},
    l4_buf_regs_t,
    l4_msg_item_consts_t::*,
    l4_msg_regs_t, l4_utcb, l4_utcb_br, l4_utcb_mr_u, l4_utcb_t,
    L4_fpage_rights::*,
    L4_fpage_type::*,
};

use crate::cap::{Cap, Interface};
use crate::error::{Error, GenericErr, Result};
use crate::ipc::Serialisable;
use crate::types::{Mword, UMword};
use num::{cast::ToPrimitive, NumCast};

/// UTCB constants (architecture-specific)
pub use crate::sys::consts::UtcbConsts as Consts;

/// Number of words used by an item when written to the message registers
pub const WORDS_PER_ITEM: usize = 2;

#[macro_export]
macro_rules! utcb_mr {
    ($utcb:ident <- $($arg:expr),*) => (
        $(println!("{}", $arg + $arg);)*
    )
}

// see union l4_msg_regs_t
const MSG_REG_COUNT: usize = UTCB_GENERIC_DATA_SIZE / (size_of::<UMword>() / size_of::<UMword>());

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
    /// Get a reference to the current UTCB.
    ///
    /// # Safety
    ///
    /// The caller must make sure that the UTCB exists exactly once, otherwise concurrent
    /// modifications and invalidation of references to the UTCB can occur.
    pub unsafe fn current() -> Utcb {
        unsafe {
            // this can't fail: no UTCB, no thread -> no execution
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
        unsafe { UtcbMr::from_utcb(self.raw) }
    }

    /// Create buffer register builder
    ///
    /// This returns a builder for the buffer registers. Unlike the raw
    /// C version, this builder is capable to write byte-granular to the message
    /// registers, allowing both space and performance-optimisations due to
    /// alignment. See [UtcbBr](struct.UtcbBr.html) for more details.
    #[inline]
    pub fn br(&mut self) -> UtcbBr {
        unsafe { UtcbBr::from_utcb(self.raw) }
    }

    #[inline(always)]
    pub fn bdr(&self) -> usize {
        unsafe { (*l4_utcb_br()).bdr as usize }
    }

    pub fn raw(&mut self) -> *mut l4_utcb_t {
        self.raw
    }
}

impl From<*mut l4_utcb_t> for Utcb {
    fn from(utcb: *mut l4_utcb_t) -> Utcb {
        Utcb { raw: utcb }
    }
}


/// Flex page types
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum FpageType {
    Special = (L4_FPAGE_SPECIAL as u8) << 4,
    Memory = (L4_FPAGE_MEMORY as u8) << 4,
    Io = (L4_FPAGE_IO as u8) << 4,
    Obj = (L4_FPAGE_OBJ as u8) << 4,
}

/// capability mapping type (flex page type)
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum MapType {
    Map = L4_MAP_ITEM_MAP as u8,
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
    fpage: u64,
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
            fpage,
        }
    }

    pub fn from_cap<T: Interface>(
        c: &Cap<T>,
        rights: FpageRights,
        mt: Option<MapType>,
    ) -> SndFlexPage {
        SndFlexPage::new(
            unsafe { ::l4_sys::l4_obj_fpage(c.raw(), 0, rights.bits() as u8).raw },
            0,
            mt,
        )
    }
}

/// Align the given pointer
///
/// # Safety
///
/// The given pointer MUST stay within the allocated bounds and is UB otherwise.
#[inline(always)]
unsafe fn align<T>(ptr: *mut u8) -> *mut u8 {
    let new_offset = ptr.align_offset(align_of::<T>());
    // SAFETY: the given pointer MUST stay within the allocated object.
    unsafe {
        ptr.add(new_offset)
    }
}

// make the dumb C types more smart: let them know their limits
pub trait UtcbRegSize {
    // size in bytes
    const BUF_SIZE: usize;
}

impl UtcbRegSize for l4_msg_regs_t {
    const BUF_SIZE: usize = UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize * size_of::<UMword>();
}

impl UtcbRegSize for l4_buf_regs_t {
    const BUF_SIZE: usize = UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE as usize * size_of::<UMword>();
}

/// Convenience interface for message serialisation to the message registers
///
/// This thin type wraps the pointer to the message reigsters,  offering type-aligned,
/// byte-granular write and read access. It also does offset accounting, so that data only needs to
/// be stuffed in or dragged out. It is not meant to be used directly, but through the more
/// convenient and safe [UtcbMr](struct.UtcbMr.html) and [UtcbBr](struct.UtcbBr.html) types.
pub struct Registers<B: UtcbRegSize> {
    buf: *mut u8, // casted u64 pointer to msg or buf registers
    base: *mut u8,
    size: PhantomData<B>,
}

impl<U: UtcbRegSize> Registers<U> {
    /// Initialise struct from given pointer
    ///
    /// The pointer must be valid.
    pub unsafe fn from_raw(buf: *mut u64) -> Self {
        let base = buf as *mut _ as *mut u8;
        Registers {
            base,
            buf: base,
            size: PhantomData,
        }
    }

    /// Write given value to the next free, type-aligned slot
    ///
    /// The value is aligned and written into the message registers.
    #[inline]
    pub fn write<T: Serialisable>(&mut self, val: T) -> Result<()> {
        // SAFETY: The ponter must be checked for the bounds of the object beforehand.
        let ptr = unsafe {
            align::<T>(self.buf)
        };
        // SAFETY: The ponter must be checked for the bounds of the object beforehand.
        let next = unsafe {
            ptr.add(size_of::<T>())
        };
        l4_err_if!((next as usize - self.base as usize) > U::BUF_SIZE
                   => Generic, MsgTooLong);
        // SAFETY: Bounds and alignment have been checked.
        unsafe {
            *(ptr as *mut T) = val;
        }
        self.buf = next;
        Ok(())
    }

    /// Read given type from the memory region at the internal buffer + offset
    ///
    /// This function reads from the given pointer + the internal offset a value
    /// from the specified type. If a read beyond the message registers is
    /// attempted, an error is returned.
    #[inline]
    pub fn read<T: Serialisable>(&mut self) -> Result<T> {
        // SAFETY: ToDo, this is unsafe unless bounds are checked
        let ptr = unsafe {
            align::<T>(self.buf)
        };
        // SAFETY: ToDo, this is unsafe unless bounds are checked
        let next = unsafe {
            ptr.add(size_of::<T>())
        };
        l4_err_if!((next as usize - self.base as usize) > U::BUF_SIZE
                   => Generic, MsgTooLong);
        // SAFETY: ToDo, this is unsafe unless bounds are checked
        let val: T = unsafe {
            (*(ptr as *mut T)).clone()
        };
        self.buf = next;
        Ok(val)
    }

    /// Write a slice
    pub fn write_slice<Len, T>(&mut self, val: &[T]) -> Result<()>
    where
        Len: Serialisable + NumCast,
        T: Serialisable,
    {
        // SAFETY: this is safe as the pointer calculations are not used for accessing an object,
        // but only for its bounds calculation.
        let (buf_start, buf_end) = unsafe {
            let ptr = align::<Len>(self.buf);
            let ptr = ptr.add(1);
            // ToDo: does C align?
            let buf_start = align::<T>(ptr);
            (buf_start, buf_start.add(size_of::<T>() * val.len()))
        };
        l4_err_if!(buf_end as usize - self.base as usize > U::BUF_SIZE
                   => Generic, MsgTooLong);
        // SAFETY: The bounds and the alignment has been checked above.
        unsafe {
            self.write::<Len>(NumCast::from(val.len()).ok_or(Error::Generic(GenericErr::InvalidArg))?)?;
            val.as_ptr()
                .copy_to_nonoverlapping(buf_start as *mut T, val.len());
        }
        self.buf = buf_end; // move pointer
        Ok(())
    }

    /// Serialise a &str as a framework-compatible string.
    pub fn write_str(&mut self, val: &str) -> Result<()> {
        self.write_slice::<usize, u8>(val.as_bytes())
    }

    /// Read a slice from the registers
    ///
    /// This function reads a slice from the registers, compatible to the C++ Array types.
    ///
    /// # Safety
    ///
    /// The slice is a direct view into the registers and hence highly unsafe. A system call may
    /// directly overwrite the data that this slice points to.
    ///
    /// The caller must make sure that the data is not overwritten by a syscall, or by other parts
    /// of the code in the current application.
    pub unsafe fn read_slice<Len, T>(&mut self) -> Result<&[T]>
    where
        Len: Serialisable + ToPrimitive,
        T: Serialisable,
    {
        let len = ToPrimitive::to_usize(&self.read::<Len>()?)
            .ok_or(Error::Generic(GenericErr::InvalidArg))?;
        let ptr = align::<T>(self.buf);
        let end = ptr.add(size_of::<T>() * len);
        l4_err_if!(end as usize - self.base as usize > U::BUF_SIZE => Generic, OutOfBounds);
        self.buf = end; // advance offset behind last element
        Ok(core::slice::from_raw_parts(ptr as *const _, len))
    }

    /// Read a str from the message registers
    ///
    /// This is not a copy, but operates on the bytes of the message registers.
    /// Copy this string ASAP and at least before the next IPC takes place.
    /// 0-bytes from C strings will be discarded.
    ///
    /// # Safety
    ///
    /// See [Self::read_slice].
    pub unsafe fn read_str<'a>(&mut self) -> Result<&'a str> {
        let len = self.read::<usize>()?;
        let end = self.buf.add(len);
        l4_err_if!(end as usize - self.base as usize > U::BUF_SIZE => Generic, OutOfBounds);
        let str = core::str::from_utf8(core::slice::from_raw_parts(
            self.buf, // ↓ match 0-byte (C str?)
            match *end.offset(-1) {
                0 => len - 1,
                _ => len,
            },
        ))
        .map_err(|e| crate::error::Error::InvalidEncoding(Some(e)))?;
        self.buf = end; // advance offset behind last element
        Ok(str)
    }

    /// Mwords written to this buffer (rounded up)
    #[inline]
    pub fn words(&self) -> u32 {
        let offset = self.buf as usize - self.base as usize;
        (offset / size_of::<Mword>()
            + match offset % size_of::<usize>() {
                0 => 0,
                _ => 1,
            }) as u32
    }

    /// Reset internal offset to beginning of message registers
    #[inline]
    pub fn reset(&mut self) {
        self.buf = self.base;
    }

    /// Skip the amount of memory that the given type would occupy
    pub fn skip<T: Serialisable>(&mut self) -> Result<()> {
        let next = unsafe { align::<T>(self.buf).add(size_of::<T>()) };
        l4_err_if!(next as usize - self.base as usize > U::BUF_SIZE
                   => Generic, MsgTooLong);
        self.buf = next;
        Ok(())
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
    pub fn write_item<T: Serialisable>(&mut self, val: T) -> Result<()> {
        self.regs.write(val)?;
        self.items += 1;
        Ok(())
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

    /// Reset state to start operations from beginning of registers
    pub fn reset(&mut self) {
        self.items = 0;
        self.regs.reset();
    }
}

impl Deref for UtcbMr {
    type Target = Registers<l4_msg_regs_t>;
    fn deref(&self) -> &Registers<l4_msg_regs_t> {
        &self.regs
    }
}

impl DerefMut for UtcbMr {
    fn deref_mut(&mut self) -> &mut Registers<l4_msg_regs_t> {
        &mut self.regs
    }
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
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for UtcbBr {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}
