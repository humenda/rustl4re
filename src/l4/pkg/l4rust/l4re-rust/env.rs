use crate::sys::l4re_env_get_cap;
use l4::cap::{Cap, IfaceInit};

/// Retrieve a capability from the initial capability set of a L4Re task
pub fn get_cap<T: IfaceInit>(name: &str) -> Option<Cap<T>> {
    l4re_env_get_cap(name).map(|cap_idx| Cap::<T>::new(cap_idx))
}

/*
/// Retrieves the initial memory allocator
///
/// Each L4Re task comes with an initial memory allocator by which the task can
/// request more memory. This function gets the capability to it.
#[inline]
pub fn mem_alloc() -> l4::cap::Cap<MemAlloc> {
    unsafe { // pointer is always valid
        l4::cap::Cap::<MemAlloc>::new((*l4re_env()).mem_alloc)
    }
}
*/
