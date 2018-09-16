use core::ops::{Deref, DerefMut};
use l4_sys::{self,
        l4_cap_consts_t::{L4_CAP_SHIFT, L4_INVALID_CAP_BIT},
        l4_cap_idx_t};

use error::{Error, Result};
use types::UMword;

/// C capability index for FFI interaction.
pub type CapIdx = l4_cap_idx_t;

pub trait CapKind { }


/// Ability for a type to initialise a capability interface.
pub trait CapInit: CapKind {
    /// Initialise a CapKind type with the capability selector from the
    /// enclosing `Cap` type.
    fn new(parent: CapIdx) -> Self;
}

/// Untyped Capability
pub struct Untyped;

impl CapKind for Untyped { }

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
pub struct Cap<T: CapKind> {
    pub(crate) interface: T,
    pub(crate) raw: CapIdx,
}

/// As long as this isn't atomically ref-counted, forbid sending
//impl<T: ?Sized> !marker::Send for Rc<T> {}
//impl<T: ?Sized> !marker::Sync for Rc<T> {}

impl<T: CapKind> Deref for Cap<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.interface
    }
}

impl<T: CapKind> DerefMut for Cap<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.interface
    }
}

impl<T: CapKind> PartialEq<Cap<T>> for Cap<T> {
    fn eq(&self, other: &Cap<T>) -> bool {
        (self.raw >> L4_CAP_SHIFT as u64) == (other.raw >> L4_CAP_SHIFT as u64)
    }

    fn ne(&self, other: &Cap<T>) -> bool {
        (self.raw >> L4_CAP_SHIFT as u64) != (other.raw >> L4_CAP_SHIFT as u64)
    }
}

impl<T: CapKind> PartialEq<CapIdx> for Cap<T> {
    fn eq(&self, other: &CapIdx) -> bool {
        (self.raw >> L4_CAP_SHIFT as usize) == (other >> L4_CAP_SHIFT as usize)
    }

    fn ne(&self, other: &CapIdx) -> bool {
        (self.raw >> L4_CAP_SHIFT as usize) != (other >> L4_CAP_SHIFT as usize)
    }
}


impl<T: CapKind> Cap<T> {
    pub fn new(c: CapIdx, protocol: T) -> Cap<T> {
        Cap { raw: c, interface: protocol }
    }

    pub fn from(c: CapIdx) -> Cap<Untyped> {
        Cap { raw: c, interface: Untyped }
    }

    /// Allocate new capability slot
    pub fn alloc(&mut self) -> Result<Cap<Untyped>> {
        let cap = Cap {
            interface: Untyped,
            raw: unsafe { l4_sys::l4re_util_cap_alloc() }
        };
        match cap.is_valid() {
            true => Ok(cap),
            false => Err(Error::InvalidCap),
        }
    }

    #[inline]
    pub fn is_valid(&self) -> bool {
        (self.raw & L4_INVALID_CAP_BIT as u64) == 0
    }

    #[inline]
    pub fn is_invalid(&self) -> bool {
        (self.raw & L4_INVALID_CAP_BIT as u64) != 0
    }

    /// Get a raw (OS) capability handle
    ///
    /// This function returns the raw capability handle, used to interface with
    /// the kernel and other processes. This function is marked as unsafe,
    /// because this handle allows bypassing the safety guarantees provided by
    /// the `Cap<T>` type.
    #[inline]
    pub unsafe fn raw(&self) -> CapIdx {
    self.raw
    }
}

