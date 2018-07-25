use l4_sys::{self, L4_CAP_SHIFT, L4_INVALID_CAP_BIT, l4_cap_idx_t};
use std::ops::Deref;

use error::{Error, Result};

/// C capability index for FFI interaction.
pub type CapIdx = l4_cap_idx_t;

pub trait CapKind { }

/// Untyped Capability
pub struct Untyped;

impl CapKind for Untyped { }

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
pub struct Cap<T: CapKind> {
    interface: T,
    pub raw: l4_cap_idx_t
}

impl<T: CapKind> Deref for Cap<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.interface
    }
}

impl<T: CapKind> PartialEq<Cap<T>> for Cap<T> {
    fn eq(&self, other: &Cap<T>) -> bool {
        (self.raw >> L4_CAP_SHIFT) == (other.raw >> L4_CAP_SHIFT)
    }

    fn ne(&self, other: &Cap<T>) -> bool {
        (self.raw >> L4_CAP_SHIFT) != (other.raw >> L4_CAP_SHIFT)
    }
}

impl<T: CapKind> PartialEq<CapIdx> for Cap<T> {
    fn eq(&self, other: &CapIdx) -> bool {
        (self.raw >> L4_CAP_SHIFT) == (other >> L4_CAP_SHIFT)
    }

    fn ne(&self, other: &CapIdx) -> bool {
        (self.raw >> L4_CAP_SHIFT) != (other >> L4_CAP_SHIFT)
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
        (self.raw & L4_INVALID_CAP_BIT) == 0
    }

    #[inline]
    pub fn is_invalid(&self) -> bool {
        (self.raw & L4_INVALID_CAP_BIT) != 0
    }
}

