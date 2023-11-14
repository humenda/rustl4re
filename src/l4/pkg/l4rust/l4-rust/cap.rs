use core::ops::{Deref, DerefMut};
use l4_sys::{
    self,
    l4_cap_consts_t::{L4_CAP_SHIFT, L4_INVALID_CAP_BIT},
    l4_cap_idx_t,
    l4_default_caps_t::L4_BASE_TASK_CAP,
    l4_msg_item_consts_t::L4_MAP_ITEM_GRANT,
    l4_task_map,
    L4_cap_fpage_rights::L4_CAP_FPAGE_RWSD,
};

use crate::error::{Error, Result};

/// C capability index for FFI interaction.
pub type CapIdx = l4_cap_idx_t;

/// Capability Interface Marker
///
/// This marker trait identifies an object to provide a service through a capability.
pub trait Interface {
    fn raw(&self) -> CapIdx;
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
    #[inline]
    fn raw(&self) -> CapIdx {
        self.0
    }
}

impl IfaceInit for Untyped {
    fn new(parent: CapIdx) -> Self {
        Untyped(parent)
    }
}

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
pub struct Cap<T: Interface> {
    pub(crate) interface: T,
}

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
        (self.interface.raw() >> L4_CAP_SHIFT as u64) == (other.raw() >> L4_CAP_SHIFT as u64)
    }

    fn ne(&self, other: &Cap<T>) -> bool {
        (self.interface.raw() >> L4_CAP_SHIFT as u64) != (other.raw() >> L4_CAP_SHIFT as u64)
    }
}

impl<T: Interface> PartialEq<CapIdx> for Cap<T> {
    fn eq(&self, other: &CapIdx) -> bool {
        (self.interface.raw() >> L4_CAP_SHIFT as usize) == (other >> L4_CAP_SHIFT as usize)
    }

    fn ne(&self, other: &CapIdx) -> bool {
        (self.interface.raw() >> L4_CAP_SHIFT as usize) != (other >> L4_CAP_SHIFT as usize)
    }
}

impl<T: Interface + IfaceInit> Cap<T> {
    pub fn new(c: CapIdx) -> Cap<T> {
        Cap {
            interface: T::new(c),
        }
    }
}

impl<T: Interface> Cap<T> {
    pub fn from_raw(val : T) -> Cap<T> {
        Cap {
            interface: val
        }
    }

    pub fn cast<U: Interface + IfaceInit>(self) -> Cap<U> {
        Cap {
            interface: U::new(self.raw()),
        }
    }

    #[inline]
    pub fn is_valid(&self) -> bool {
        (self.interface.raw() & L4_INVALID_CAP_BIT as u64) == 0
    }

    #[inline]
    pub fn is_invalid(&self) -> bool {
        (self.interface.raw() & L4_INVALID_CAP_BIT as u64) != 0
    }

    /// Get a raw (OS) capability handle
    ///
    /// This function returns the raw capability handle, used to interface with
    /// the kernel and other processes. This function is marked as unsafe,
    /// because this handle allows bypassing the safety guarantees provided by
    /// the `Cap<T>` type.
    #[inline]
    pub fn raw(&self) -> CapIdx {
        self.interface.raw()
    }

    /// Transfer a given capability into the capability slot (AKA index) of **this** capability.
    ///
    /// This will consume the given source capability and transfer (move) it into the slot of this
    /// one. `transfer` will consume `self`, allowing an capability interface change along the way.  
    /// If an invalid source capability is passed or this capability is invalid, an
    /// `Error::InvalidCap` is returned.
    ///
    /// This function is unsafe since the kernel does not care what is in the slots and will carry
    /// out the operation nevertheless. So if you transfer a capability from source to this slot, the
    /// capability from this slot will be ***lost***. From Rust's **memory-safety**-perspective,
    /// the function is safe.
    pub unsafe fn transfer(&mut self, source: Cap<T>) -> Result<()> {
        if !self.is_valid() || !source.is_valid() {
            return Err(Error::InvalidCap);
        }
        l4_task_map(
            L4_BASE_TASK_CAP as u64,
            L4_BASE_TASK_CAP as u64,
            l4_sys::l4_obj_fpage(source.raw(), 0, L4_CAP_FPAGE_RWSD as u8),
            self.send_base(L4_MAP_ITEM_GRANT as u32, None) as super::sys::l4_addr_t | 0xe0,
        );
        Ok(())
    }

    /// Return send base for this object
    ///
    /// This allows the specification of the send base for an object (default is this object
    /// itself). If the first parameter is set, the object will be **granted**.
    /// In other words, it is possible to specify the object space location of this capability for
    /// map operations.
    fn send_base(&self, grant: u32, base_cap: Option<l4_cap_idx_t>) -> u64 {
        let base_cap = base_cap.unwrap_or_else(|| self.raw());
        l4_sys::l4_map_obj_control(base_cap, grant)
    }
}

/// Short-hand to construct a Cap from an index.
pub fn from(c: CapIdx) -> Cap<Untyped> {
    Cap {
        interface: Untyped { 0: c },
    }
}

/// Construct an invalid cap
#[inline]
pub const fn invalid_cap() -> Cap<Untyped> {
    Cap {
        interface: Untyped(l4_sys::l4_cap_consts_t::L4_INVALID_CAP as u64),
    }
}
