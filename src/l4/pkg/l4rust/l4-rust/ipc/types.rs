//! IPC Interface Types
use _core::{ptr::NonNull, mem::transmute};

use super::super::{
    cap::{Cap, CapIdx, IfaceInit, Interface, invalid_cap},
    error::{Error, GenericErr, Result},
    ipc::MsgTag,
    ipc::serialise::{Serialisable, Serialiser},
    types::UMword,
    utcb::UtcbMr
};
use l4_sys::{consts::UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE,
    l4_buf_regs_t,
    l4_cap_consts_t::{L4_INVALID_CAP_BIT, L4_CAP_MASK},
    l4_msg_item_consts_t::{L4_RCV_ITEM_SINGLE_CAP, L4_RCV_ITEM_LOCAL_ID},
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
    fn dispatch(&mut self, tag: MsgTag, _: &mut UtcbMr,
                _: &mut BufferAccess) -> Result<MsgTag>;
}

/// Empty marker trait for server-side message dispatch
///
/// Types implementing this trait **must** make sure that their first element is
/// a function pointer pointing to the dispatch method of the
/// [Dispatch trait](trait.Dispatch.html); use `#[repr(C)]` for a guaranteed
/// struct member order.  
/// The function pointer must have the type
/// `fn(&mut self, MsgTag, *mut l4_utcb_t) -> Result<MsgTag>`.
pub unsafe trait Callable: Dispatch { }

pub trait Demand {
    const BUFFER_DEMAND: u32;
}

/// Define a function in a type and allow the derivation of the dual type
///
/// This allows the macro rules to define the type for a sender and derive the receiver part
/// automatically.
pub trait HasDual {
    type Dual;
}

pub trait ReadReturn<T> {
    fn read(mr: &mut UtcbMr) -> T;
}

// allow the auto-derived client implementation to expose its buffer manager. This trait is hidden
// since it is unused by the server and shouldn't be touched by the end user.
#[doc(hidden)]
pub trait CapProviderAccess {
    unsafe fn access_buffers(&mut self) -> BufferAccess {
        unimplemented!();
    }
}

pub struct BufferAccess {
    pub(crate) ptr: Option<NonNull<CapIdx>>
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
pub trait CapProvider  {
    fn new() -> Self;

    /// Retrieve capability at allocated buffer slot/index
    ///
    /// The argument must be within `0 <= index <= self.caps_used()`.
    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
                where T: Interface + IfaceInit;

    /// As `rcv_cap`, without bounds checks.
    unsafe fn rcv_cap_unchecked<T>(&self, index: usize) -> Cap<T>
                where T: Interface + IfaceInit;
    /// Return the amount of allocated buffer slots.
    #[inline]
    fn caps_used(&self) -> u32;

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
    fn alloc_capslots(&mut self, demand: u32) -> Result<()>;

    /// Set the receive flags for the buffers.
    ///
    /// This must be called **before** any buffer has been allocated and fails otherwise.
    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()>;

    /// Return the maximum number of slots this manager could allocate
    #[inline]
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
    #[inline]
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
        Bufferless { }
    }

    fn alloc_capslots(&mut self, demand: u32) -> Result<()> {
        match demand == 0 {
            true => Ok(()),
            false => Err(Error::InvalidArg("buffer allocation not permitted", None)),
        }
    }

    fn rcv_cap<T>(&self, _: usize) -> Result<Cap<T>>
                where T: Interface + IfaceInit {
        Err(Error::InvalidArg("No buffers allocated", None))
    }

    /// Returns the invalid cap
    unsafe fn rcv_cap_unchecked<T>(&self, _: usize) -> Cap<T>
                where T: Interface + IfaceInit {
        invalid_cap().cast::<T>()
    }

    fn set_rcv_cap_flags(&mut self, _: UMword) -> Result<()> { Ok(()) }

    #[inline]
    fn max_slots(&self) -> u32 { 0 }
    #[inline]
    fn caps_used(&self) -> u32 { 0 }
    fn setup_wait(&mut self, _: *mut l4_utcb_t) { }

    /// a non-usable buffer access, containing no pointer
    #[inline]
    unsafe fn access_buffers(&mut self) -> BufferAccess {
        BufferAccess { ptr: None }
    }
}

pub struct BufferManager {
    /// number of allocated capabilities
    caps: u32,
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

    fn alloc_capslots(&mut self, demand: u32) -> Result<()> {
        // take two extra buffers for a possible timeout and a zero terminator
        if demand + 3 >= self.br.len() as u32 {
            return Err(Error::InvalidArg("Capability slot demand too large",
                                         Some(demand as isize)));
        }
        // ToDo: set up is wrong, +1 caps allocated than actually required
        while demand + 1 > self.caps {
            let cap = unsafe { l4_sys::l4re_util_cap_alloc() };
            if (cap & L4_INVALID_CAP_BIT as u64) != 0 {
                return Err(Error::Generic(GenericErr::NoMem));
            }

            // safe this cap as a "receive item" in the format that the kernel
            // understands
            self.br[self.caps as usize] = cap | L4_RCV_ITEM_SINGLE_CAP as u64
                    | self.cap_flags;
            self.caps += 1;
        }
        self.br[self.caps as usize] = 0; // terminate receive list
        Ok(())
    }

    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
            where T: Interface + IfaceInit {
        match index >= self.caps as usize {
            true  => Err(Error::InvalidArg("Index not allocated", Some(index as isize))),
            false => Ok(Cap::<T>::new(self.br[index] & L4_CAP_MASK as u64)),
        }
    }

    unsafe fn rcv_cap_unchecked<T>(&self, index: usize) -> Cap<T>
            where T: Interface + IfaceInit {
        Cap::<T>::new(self.br[index] & L4_CAP_MASK as u64)
    }


    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()> {
        if self.cap_flags == 0 {
            self.cap_flags = flags as u64;
            return Ok(());
        }
        Err(Error::InvalidState("Unable to set buffer flags when capabilities \
                                were already allocated"))
    }

    #[inline]
    fn max_slots(&self) -> u32 {
        self.br.len() as u32
    }

    #[inline]
    fn caps_used(&self) -> u32 {
        self.caps
    }

    fn setup_wait(&mut self, u: *mut l4_utcb_t) {
        unsafe {
            let br: *mut l4_buf_regs_t = l4_utcb_br_u(u);
            (*br).bdr = 0;
            for index in 0usize..(self.caps as usize - 1) {
                (*br).br[index] = self.br[index];
            }
        }
    }

    /// Get raw access to the allocated buffers
    /// Using a method on the BufferManager must make sure that they do
    /// not exceed the allocated buffer of this type (BufferManager).
    #[inline]
    unsafe fn access_buffers(&mut self) -> BufferAccess {
        BufferAccess {
            ptr: Some(NonNull::new_unchecked(self.br.as_mut_ptr()))
        }
    }
}

pub struct Array<'a, Len, T> {
    inner: &'a [T],
    // (l4) c++ arrays use different slice length by default
    _len_type: ::core::marker::PhantomData<Len>,
}

unsafe impl<'a, Len, T> Serialiser for Array<'a, Len, T> 
        where Len: num::NumCast + Serialisable, T: Serialisable{
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        Ok(Array {
            inner: transmute::<_, &'a [T]>(mr.read_slice::<Len, T>()?),
            _len_type: ::core::marker::PhantomData,
        })
    }
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        // default array in C++ is unsigned short - should be flexible
        mr.write_slice::<Len, T>(self.inner)
    }
}

impl<'a, Len, T> From<&'a [T]> for Array<'a, Len, T> 
        where Len: Serialisable + num::NumCast, T: Serialisable {
    fn from(sl: &'a [T]) -> Self {
        Array {
            inner: sl,
            _len_type: ::core::marker::PhantomData,
        }
    }
}

impl<'a, Len, T> Into<&'a [T]> for Array<'a, Len, T>
        where Len: Serialisable + num::NumCast, T: Serialisable {
    fn into(self) -> &'a [T] {
        self.inner
    }
}

/// Default C++-Compatible Array
///
/// This array uses an u16 to encode the length, a good default choice from the
/// C++ framework given the length of the message registers. Use this as the
/// default array / slice type.
pub type BufArray<'a, T> = Array<'a, u16, T>;

#[cfg(feature="std")]
impl<'a, T: Serialisable> core::convert::Into<Vec<T>> for BufArray<'a, T> {
    fn into(self) -> Vec<T> {
        let slice: &[T] = self.into();
        Vec::from(slice)
    }
}
