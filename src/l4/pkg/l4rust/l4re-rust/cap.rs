use core::ops::{Deref, DerefMut};

use l4::{
    cap::{Cap, IfaceInit},
    error::{Error, Result},
};

pub struct OwnedCap<T: IfaceInit>(Cap<T>);

impl<T: IfaceInit> OwnedCap<T> {
    /// Allocate new capability slot
    pub fn alloc() -> OwnedCap<T> {
        OwnedCap(Cap::<T>::new(unsafe { l4_sys::l4re_util_cap_alloc() }))
    }

    pub fn from(src: Cap<T>) -> Result<Self> {
        let mut cap = OwnedCap::<T>::alloc();
        unsafe {
            (*cap).transfer(src)?;
        }
        match cap.is_valid() {
            true => Ok(cap),
            false => Err(Error::InvalidCap),
        }
    }

    /// Return a copy of the internal `Cap<T>`
    pub fn cap(&self) -> Cap<T> {
        unsafe {
            Cap::<T>::new(self.0.raw())
        }
    }
}

impl<T: IfaceInit> Deref for OwnedCap<T> {
    type Target = Cap<T>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T: IfaceInit> DerefMut for OwnedCap<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<T: IfaceInit> Drop for OwnedCap<T> {
    fn drop(&mut self) {
        unsafe { // free allocated capability index
            l4::sys::l4re_util_cap_free(self.0.raw());
        }
    }
}
