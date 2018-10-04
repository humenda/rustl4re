use core::ops::{Deref, DerefMut};
use l4_sys::{self,
        l4_cap_consts_t::{L4_CAP_SHIFT, L4_INVALID_CAP_BIT},
        l4_cap_idx_t};

use error::{Error, Result};

/// C capability index for FFI interaction.
pub type CapIdx = l4_cap_idx_t;

/// Capability Interface Marker
///
/// This marker trait identifies an object to provide a service through a capability.
pub trait Interface {
    unsafe fn cap(&self) -> CapIdx;
}


/// Ability for a type to initialise a capability interface.
pub trait IfaceInit: Interface {
    /// Initialise a Interface type with the capability selector from the
    /// enclosing `Cap` type.
    fn new(parent: CapIdx) -> Self;
}

/// Untyped Capability
pub struct Untyped(CapIdx);

impl Interface for Untyped {
    unsafe fn cap(&self) -> CapIdx {
        self.0
    }
}

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
pub struct Cap<T: Interface> {
    pub(crate) interface: T,
}

/// As long as this isn't atomically ref-counted, forbid sending
//impl<T: ?Sized> !marker::Send for Rc<T> {}
//impl<T: ?Sized> !marker::Sync for Rc<T> {}

impl<T: Interface> Deref for Cap<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.interface
    }
}

impl<T: Interface> DerefMut for Cap<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.interface
    }
}

impl<T: Interface> PartialEq<Cap<T>> for Cap<T> {
    fn eq(&self, other: &Cap<T>) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as u64) == (other.raw() >> L4_CAP_SHIFT as u64)
        }
    }

    fn ne(&self, other: &Cap<T>) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as u64)
                != (other.raw() >> L4_CAP_SHIFT as u64)
        }
    }
}

impl<T: Interface> PartialEq<CapIdx> for Cap<T> {
    fn eq(&self, other: &CapIdx) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as usize) == (other >> L4_CAP_SHIFT as usize)
        }
    }

    fn ne(&self, other: &CapIdx) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as usize)
                != (other >> L4_CAP_SHIFT as usize)
        }
    }
}


impl<T: Interface + IfaceInit> Cap<T> {
    pub fn new(c: CapIdx) -> Cap<T> {
        Cap { interface: T::new(c) }
    }
}

impl<T: Interface> Cap<T> {

    /// Allocate new capability slot
    pub fn alloc(&mut self) -> Result<Cap<Untyped>> {
        let cap = Cap {
            interface: Untyped {
                0: unsafe { l4_sys::l4re_util_cap_alloc() }
            }
        };
        match cap.is_valid() {
            true => Ok(cap),
            false => Err(Error::InvalidCap),
        }
    }

    #[inline]
    pub fn is_valid(&self) -> bool {
        unsafe {
            (self.interface.cap() & L4_INVALID_CAP_BIT as u64) == 0
        }
    }

    #[inline]
    pub fn is_invalid(&self) -> bool {
        unsafe {
            (self.interface.cap() & L4_INVALID_CAP_BIT as u64) != 0
        }
    }

    /// Get a raw (OS) capability handle
    ///
    /// This function returns the raw capability handle, used to interface with
    /// the kernel and other processes. This function is marked as unsafe,
    /// because this handle allows bypassing the safety guarantees provided by
    /// the `Cap<T>` type.
    #[inline]
    pub unsafe fn raw(&self) -> CapIdx {
        self.interface.cap()
    }
}

pub fn from(c: CapIdx) -> Cap<Untyped> {
    Cap { interface: Untyped { 0: c } }
}


