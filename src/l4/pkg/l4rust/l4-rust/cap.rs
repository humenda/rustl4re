use _core::ops::{Deref, DerefMut};
use l4_sys::{self,
        l4_cap_consts_t::{L4_CAP_SHIFT, L4_INVALID_CAP_BIT},
        L4_cap_fpage_rights::L4_CAP_FPAGE_RWSD,
        l4_cap_idx_t,
        l4_default_caps_t::L4_BASE_TASK_CAP,
        l4_msg_item_consts_t::L4_MAP_ITEM_GRANT,
        l4_task_map,
};

use crate::error::{Error, Result};

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

impl IfaceInit for Untyped {
    fn new(parent: CapIdx) -> Self {
        Untyped(parent)
    }
}

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
#[derive(Clone)]
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

    pub fn cast<U: Interface + IfaceInit>(self) -> U {
        unsafe {
            U::new(self.cap())
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

    /// Transfer a given capability into this capability
    ///
    /// This will consume the given source capability and transfer (move) it into the slot of this
    /// one. `transfer` will consume `self`, allowing an capability interface change along the way.  
    /// If an invalid source capability is passed or this capability is invalid, an
    /// `Error::InvalidCap` is returned.
    ///
    /// This function is unsafe since the kernel does not care what is in the slots and will carry
    /// out the operation nevertheless. So if you move a capability from source to this slot, the
    /// capability from this slot will be ***lost***. From Rust's **memory-safety**-perspective,
    /// the fnction is safe.
    pub unsafe fn transfer<U: Interface + IfaceInit>(&mut self, source: Cap<U>)
                -> Result<Cap<U>> {
        if !self.is_valid() || source.is_valid() {
            return Err(Error::InvalidCap);
        }
        l4_task_map(L4_BASE_TASK_CAP as u64, L4_BASE_TASK_CAP as u64,
                    l4_sys::l4_obj_fpage(source.cap(), 0, L4_CAP_FPAGE_RWSD as u8),
                    self.send_base(L4_MAP_ITEM_GRANT as u64, None) | 0xe0);
        Ok(Cap::new(self.cap()))
    }

    /// Return send base for this object
    ///
    /// This allows the specification of the send base for an object (default is this object
    /// itself). If the first parameter is set, the object will be **granted**.
    /// In other words, it is possible to specify the object space location of this capability for
    /// map operations.
    fn send_base(&self, grant: u64, base_cap: Option<l4_cap_idx_t>) -> u64 {
        unsafe {
            let base_cap = base_cap.unwrap_or(self.cap());
            l4_sys::l4_map_obj_control(base_cap, grant)
        }
    }
}

pub fn from(c: CapIdx) -> Cap<Untyped> {
    Cap { interface: Untyped { 0: c } }
}

