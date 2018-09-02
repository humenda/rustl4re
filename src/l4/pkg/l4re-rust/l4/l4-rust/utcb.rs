use core::{intrinsics::transmute,
        mem::{align_of, size_of}};

use l4_sys::{l4_uint64_t, l4_umword_t,
    l4_utcb, l4_utcb_br, l4_utcb_mr, l4_utcb_mr_u, l4_utcb_t};
use l4_sys::{L4_UTCB_GENERIC_DATA_SIZE, l4_msg_regs_t};

use error::{Error, GenericErr, Result};

const UTCB_DATA_SIZE_IN_BYTES: usize = L4_UTCB_GENERIC_DATA_SIZE as usize
                    * size_of::<l4_umword_t>();

#[macro_export]
macro_rules! utcb_mr {
    ($utcb:ident <- $($arg:expr),*) => (
        $(println!("{}", $arg + $arg);)*
    )
}


// see union l4_msg_regs_t
const MSG_REG_COUNT: usize = L4_UTCB_GENERIC_DATA_SIZE as usize / 
        (size_of::<l4_uint64_t>() / size_of::<l4_umword_t>());


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
    pub fn mr(&self) -> &[u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &*(ptr as *const [u64; MSG_REG_COUNT])
        }
    }

    #[inline]
    pub fn mr_mut(&mut self) -> &mut [u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &mut *(ptr as *mut [u64; MSG_REG_COUNT])
        }
    }

    #[inline(always)]
    pub fn bdr(&self) -> usize {
        unsafe {
            (*l4_utcb_br()).bdr as usize
        }
    }
}

pub trait Serialisable: Clone { }

macro_rules! impl_serialisable {
    ($($type:ty),*) => {
        $(
            impl Serialisable for $type { }
        )*
    }
}

impl_serialisable!(i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64,
                   bool, char);

/// Return an aligned pointer to the next free message register
///
/// The pointer is advanced by the end of the already written message registers,
/// aligned to the required type. Returned is a
/// pointer to the type-aligned memory region.
#[inline]
unsafe fn align_with_offset<T>(ptr: *mut u8, offset: usize)
        -> (*mut u8, usize) {
    let ptr = ptr.offset(offset as isize);
    let new_offset = ptr.align_offset(align_of::<T>());
    (ptr.offset(new_offset as isize), offset + new_offset)
}

/// Convenience interface for message serialisation to the message registers
pub struct Msg {
    mr: *mut u8, // casted utcb_mr pointer
    offset: usize
}

impl Msg {
    pub unsafe fn from_raw_mr(mr: *mut l4_msg_regs_t) -> Msg {
        Msg {
            mr: transmute::<*mut u64, *mut u8>((*mr).mr.as_mut_ptr()),
            offset: 0
        }
    }

    pub fn new() -> Msg {
        unsafe { // safe, because *every thread MUST have* a UTCB with message registers
            Msg {
                mr: transmute::<*mut u64, *mut u8>((*l4_utcb_mr()).mr.as_mut_ptr()),
                offset: 0,
            }
        }
    }

    /// Write given value to the next free, type-aligned slot
    ///
    /// The value is aligned and written into the message registers.
    pub unsafe fn write<T: Serialisable>(&mut self, val: T) 
            -> Result<()> {
        let (ptr, offset) = align_with_offset::<T>(self.mr, self.offset);
        let next = offset + size_of::<T>();
        if next > UTCB_DATA_SIZE_IN_BYTES {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        *transmute::<*mut u8, *mut T>(ptr) = val;
        self.offset = next; // advance offset *behind* element
        Ok(())
    }

    /// Read given type from the memory region at the internal buffer + offset
    ///
    /// This function reads from the given pointer + the internal offset a value
    /// from the specified type. This is certainly unsafe, because it is not
    /// sure what will be at the given position and in the worst case a useless
    /// value will be read. Reading beyond the limits of the  message registers
    /// will yield an eror.
    pub unsafe fn read<T: Serialisable>(&mut self)  -> Result<T> {
        let (ptr, offset) = align_with_offset::<T>(self.mr, self.offset);
        let next = offset + size_of::<T>();
        if next > UTCB_DATA_SIZE_IN_BYTES {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        self.offset = next; // advance offset *behind* element
        let val: T = (*transmute::<*mut u8, *mut T>(ptr)).clone();
        Ok(val)
    }
}


