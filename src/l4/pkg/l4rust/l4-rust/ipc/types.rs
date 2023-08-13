//! IPC Interface Types
use core::ptr::NonNull;

use super::super::{
    cap::{invalid_cap, Cap, CapIdx, IfaceInit, Interface},
    error::{Error, GenericErr, Result},
    ipc::serialise::{Serialisable, Serialiser},
    ipc::MsgTag,
    l4_err,
    types::UMword,
    utcb::UtcbMr,
};
use l4_sys::{
    consts::UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE,
    l4_buf_regs_t,
    l4_cap_consts_t::{L4_CAP_MASK, L4_INVALID_CAP_BIT},
    l4_msg_item_consts_t::{L4_RCV_ITEM_LOCAL_ID, L4_RCV_ITEM_SINGLE_CAP},
    l4_utcb_br_u, l4_utcb_t,
};
#[cfg(feature = "std")]
use std::vec::Vec;

/// IPC Dispatch Delegation Functionality
///
/// When implementing the server-side logic for an IPC service, the user needs
/// to implement all required IPC functions and register it with the server
/// loop. When a new message arrives, the user implementationneeds to dispatch
/// the message to the correct IPC implementation. This dispatch information is
/// auto-generated in the interface. Therefore this trait provides a default
/// implementation to delegate the dispatching to the auto-derived interface
/// definition, passing the actual arguments back to the user implementation,
/// once read from the UTCB.
pub trait Dispatch {
    fn dispatch(&mut self, tag: MsgTag, _: &mut UtcbMr, _: &mut BufferAccess) -> Result<MsgTag>;
}

/// Empty marker trait for server-side message dispatch
///
/// Types implementing this trait **must** make sure that their first element is
/// a function pointer pointing to the dispatch method of the
/// [Dispatch trait](trait.Dispatch.html); use `#[repr(C)]` for a guaranteed
/// struct member order.  
/// The function pointer must have the type
/// `fn(&mut self, MsgTag, *mut l4_utcb_t) -> Result<MsgTag>`.
/// To avoid moves of the object, a struct should also be pinned, e.g. by containing a
/// core::marker::PhantomPinned.
pub unsafe trait Callable: Dispatch {}

pub trait Demand {
    const CAP_DEMAND: u8;
}

// allow the auto-derived client implementation to expose its buffer manager. This trait is hidden
// since it is unused by the server and shouldn't be touched by the end user.
#[doc(hidden)]
pub trait CapProviderAccess {
    unsafe fn access_buffers(&mut self) -> BufferAccess;
}

pub struct BufferAccess {
    pub(crate) ptr: Option<NonNull<CapIdx>>,
}

impl BufferAccess {
    pub unsafe fn rcv_cap_unchecked<T: IfaceInit>(&mut self) -> Cap<T> {
        let ptr = self.ptr.unwrap().as_ptr();
        let read_cap: Cap<T> = Cap::new(*ptr);
        self.ptr = Some(NonNull::new_unchecked(ptr));
        read_cap
    }
}

/// Receive window management for IPC server loop(s)
///
/// A buffer manager controls the capability slot allocation and the buffer register setup to
/// receive and dispatch kernel **items** such as capability flexpages. How and whether
/// capabilities (etc.) are allocated depends on the policy of each implementor.
pub trait CapProvider {
    fn new() -> Self;

    /// Retrieve capability at allocated buffer slot/index
    ///
    /// The argument must be within `0 <= index <= self.caps_used()`.
    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
    where
        T: Interface + IfaceInit;

    /// As `rcv_cap`, without bounds checks.
    unsafe fn rcv_cap_unchecked<T>(&self, index: usize) -> Cap<T>
    where
        T: Interface + IfaceInit;
    /// Return the amount of allocated buffer slots.
    fn caps_used(&self) -> u8;

    /// Allocate buffers according to given demand
    ///
    /// This function will allocate as many buffers as required for the given
    /// capability demand. If buffers were allocated before, these are left
    /// unchanged, only the additional buffers are allocated. If the  given
    /// demand is smaller than the actual number of allocated buffers, no action
    /// is performed.  
    /// A capability beyond the internal buffer size will resultin an
    /// `Error::InvalidArg` and capability allocation failures in an
    /// `Error::NoMem`.
    /// **Note:** memory or I/O flexpages are not supported.
    fn alloc_capslots(&mut self, cap_demand: u8) -> Result<()>;

    /// Set the receive flags for the buffers.
    ///
    /// This must be called **before** any buffer has been allocated and fails otherwise.
    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()>;

    /// Return the maximum number of slots this manager could allocate
    fn max_slots(&self) -> u32;

    /// Set up the UTCB for receiving items
    ///
    /// This function instructs where and how to map flexpages on arrival. It is
    /// usually called by the server loop.
    fn setup_wait(&mut self, _: *mut l4_utcb_t);

    /// Get a pointer to the underlying buffer
    ///
    /// This is highly unsafe and only advised in situation where the callee can
    /// make sure that there is a buffer in the buffer manager implementation
    /// and that it stores enough allocated items. It is undefined behaviour
    /// otherwise.
    unsafe fn access_buffers(&mut self) -> BufferAccess;
}

/// A buffer manager not able to receive any capability.
///
/// While this limits the server implementations, it might be a handy
/// optimisation, saving any buffer setup. Since this is a zero-sized struct,
/// it'll also shrink the memory footprint of the service.
pub struct Bufferless;

impl CapProvider for Bufferless {
    fn new() -> Self {
        Bufferless {}
    }

    fn alloc_capslots(&mut self, cap_demand: u8) -> Result<()> {
        if cap_demand == 0 {
            Ok(())
        } else {
            Err(Error::InvalidArg("buffer allocation not permitted", None))
        }
    }

    fn rcv_cap<T>(&self, _: usize) -> Result<Cap<T>>
    where
        T: Interface + IfaceInit,
    {
        Err(Error::InvalidArg("No buffers allocated", None))
    }

    /// Returns the invalid cap
    unsafe fn rcv_cap_unchecked<T>(&self, _: usize) -> Cap<T>
    where
        T: Interface + IfaceInit,
    {
        invalid_cap().cast::<T>()
    }

    fn set_rcv_cap_flags(&mut self, _: UMword) -> Result<()> {
        Ok(())
    }

    #[inline]
    fn max_slots(&self) -> u32 {
        0
    }
    #[inline]
    fn caps_used(&self) -> u8 {
        0
    }
    fn setup_wait(&mut self, _: *mut l4_utcb_t) {}

    /// a non-usable buffer access, containing no pointer
    #[inline]
    unsafe fn access_buffers(&mut self) -> BufferAccess {
        BufferAccess { ptr: None }
    }
}

pub struct BufferManager {
    /// number of allocated capabilities
    caps: u8,
    cap_flags: u64,
    // ToDo: reimplement this with the (ATM unstable) const generics
    br: [u64; l4_sys::consts::UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE as usize],
}

impl CapProvider for BufferManager {
    fn new() -> Self {
        BufferManager {
            caps: 0,
            cap_flags: L4_RCV_ITEM_LOCAL_ID as u64,
            br: [0u64; L4_UTCB_GENERIC_BUFFERS_SIZE as usize],
        }
    }

    fn alloc_capslots(&mut self, cap_demand: u8) -> Result<()> {
        // take two extra buffers for a possible timeout and a zero terminator (taken from the c++
        // version)
        if cap_demand + 3 >= self.br.len() as u8 {
            return Err(Error::InvalidArg(
                "Capability slot demand too large",
                Some(cap_demand as isize),
            ));
        }
        // ToDo: set up is wrong, +1 caps allocated than actually required
        while cap_demand + 1 > self.caps {
            let cap = unsafe { l4_sys::l4re_util_cap_alloc() };
            if (cap & L4_INVALID_CAP_BIT as u64) != 0 {
                return Err(Error::Generic(GenericErr::NoMem));
            }

            // safe this cap as a "receive item" in the format that the kernel
            // understands
            self.br[self.caps as usize] = cap | L4_RCV_ITEM_SINGLE_CAP as u64 | self.cap_flags;
            self.caps += 1;
        }
        self.br[self.caps as usize] = 0; // terminate receive list
        Ok(())
    }

    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
    where
        T: Interface + IfaceInit,
    {
        if index < self.caps as usize {
            return Ok(Cap::<T>::new(self.br[index] & L4_CAP_MASK as u64));
        }
        Err(Error::InvalidArg(
            "Index not allocated",
            Some(index as isize),
        ))
    }

    unsafe fn rcv_cap_unchecked<T>(&self, index: usize) -> Cap<T>
    where
        T: Interface + IfaceInit,
    {
        Cap::<T>::new(self.br[index] & L4_CAP_MASK as u64)
    }

    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()> {
        if self.cap_flags == 0 {
            self.cap_flags = flags as u64;
            return Ok(());
        }
        Err(Error::InvalidState(
            "Unable to set buffer flags when capabilities \
                                were already allocated",
        ))
    }

    #[inline]
    fn max_slots(&self) -> u32 {
        self.br.len() as u32
    }

    #[inline]
    fn caps_used(&self) -> u8 {
        self.caps
    }

    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    fn setup_wait(&mut self, u: *mut l4_utcb_t) {
        unsafe {
            let br: *mut l4_buf_regs_t = l4_utcb_br_u(u);
            (*br).bdr = 0;
            // copy prepared buffer values into buffer registers
            let caps = self.caps as usize;
            (*br).br[..caps].copy_from_slice(&self.br[..caps]);
        }
    }

    /// Get raw access to the allocated buffers
    /// Using a method on the BufferManager must make sure that they do
    /// not exceed the allocated buffer of this type (BufferManager).
    #[inline]
    unsafe fn access_buffers(&mut self) -> BufferAccess {
        BufferAccess {
            ptr: Some(NonNull::new_unchecked(self.br.as_mut_ptr())),
        }
    }
}

/// An IPC array (reference) type
///
/// This array wraps safely the access to a region in the message registers. It
/// can be treated like a slice. Operations on it are unsafe, because the
/// message registers might be overwritten at any time due to a syscall taking
/// place. This type should therefore only be used to copy data out / in **or**
/// with ***great*** care (and if performance is crucial).
///
/// Note that this array can be constructed from a slice and also supports the
/// conversion into a vector via the `Into` trait.
pub struct Array<'a, Len, T> {
    len: Len,
    start: *const T,
    _life: core::marker::PhantomData<&'a T>,
}

unsafe impl<'a, Len, T> Serialiser for Array<'a, Len, T>
where
    Len: num::NumCast + Serialisable + Copy,
    T: Serialisable,
    Self: 'a,
{
    #[inline]
    fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        // SAFETY: This can only be made safe when #9 is implemented.
        let sl = unsafe {
            mr.read_slice::<Len, T>()?
        };
        Ok(Array {
            start: sl.as_ptr(),
            // SAFETY: the IPC buffer is bounded to 32 mwords and hence the len always fits
            len: Len::from(sl.len()).ok_or(GenericErr::OutOfBounds)?,
            _life: core::marker::PhantomData,
        })
    }

    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        // SAFETY: ToDo, this awaits resolution of #9. This is safe for anything valid *outside*
        // the UTCB.
        let sl = unsafe { core::slice::from_raw_parts(self.start, self.len().to_u64().unwrap() as usize) };
        // default array in C++ is unsigned short - should be flexible
        mr.write_slice::<Len, T>(sl)
    }
}

impl<'a, Len, T> From<&'a [T]> for Array<'a, Len, T>
where
    Len: Serialisable + num::NumCast + num::FromPrimitive + Copy,
    T: Serialisable,
{
    fn from(sl: &'a [T]) -> Self {
        Array {
            start: sl.as_ptr(),
            len: Len::from_u64(sl.len() as u64).unwrap(),
            _life: core::marker::PhantomData,
        }
    }
}

#[cfg(feature = "std")]
impl<'a, T: Serialisable> core::convert::Into<Vec<T>> for BufArray<'a, T> {
    fn into(self) -> Vec<T> {
        // SAFETY: Safe only because the slice is immediately copied to a vector.
        let sl: &[T] = unsafe {
            core::slice::from_raw_parts(self.start, self.len() as usize)
        };
        let mut v = Vec::new();
        v.extend_from_slice(sl);
        v
    }
}

impl<'a, Len, T> Array<'a, Len, T>
where
    Len: Serialisable + num::NumCast + Copy,
    T: Serialisable,
{
    pub fn len(&self) -> Len {
        self.len
    }
}

/// Default C++-Compatible Array
///
/// This array uses an u16 to encode the length, a good default choice from the
/// C++ framework given the length of the message registers. Use this as the
/// default array / slice type.
pub type BufArray<'a, T> = Array<'a, u16, T>;

/// Slice-backed string type
///
/// This is a slim `&[u8]` wrapper, initialised from an `&str`.  On
/// initialisation, a `&str` is copied into its `&mut [u8]` slice. To make use
/// of the string, the `as_str` method can be used to obtain a `&str` reference.
///
/// A use case is a `&str` reference received from the framework. Since the
/// `&str` reference resides within the message registers, it could be
/// overwritten with the next syscall. Copying it to a stack-allocated buffer
/// would make a safe access possible and would get around the allocation of a
/// string.
///
/// ```
/// use l4::ipc::types::BufStr;
/// let mut on_stack = [0u8; 200];
/// let b = BufStr::new(&mut on_stack, "Optimise if appropriate.")
///         .expect("String too long for buffer");
/// ```
/// println!("Got: {}", b.as_ref());
pub struct BufStr<'a>(&'a mut [u8], usize);

impl<'a> BufStr<'a> {
    /// Initialise a buffer with a string received by an IPC call.
    ///
    /// If the buffer is shorter than the given `&str`, `l4::error::Error::GenericErr::MsgTooLong`
    /// is returned.
    pub fn new(target: &'a mut [u8], input: &str) -> Result<BufStr<'a>> {
        l4_err_if!(input.len() > target.len() => Generic, MsgTooLong);
        unsafe {
            input
                .as_bytes()
                .as_ptr()
                .copy_to(target.as_mut_ptr(), input.len());
        }
        Ok(BufStr(target, input.len()))
    }

    pub fn get(&self, i: usize) -> Option<&u8> {
        self.0.get(i)
    }

    /// Set a value at the given index
    ///
    /// The index references indices of the underlying `&[u8]`.
    /// The value is None if the referenced index is out of bounds.
    pub fn set(&mut self, i: usize, v: u8) -> Option<()> {
        self.0.get_mut(i).map(|old| *old = v)
    }
}

impl<'a> core::convert::AsRef<str> for BufStr<'a> {
    fn as_ref(&self) -> &str {
        unsafe {
            // this struct can only be initialised from a rust str
            core::str::from_utf8_unchecked(core::slice::from_raw_parts(self.0.as_ptr(), self.1))
        }
    }
}
