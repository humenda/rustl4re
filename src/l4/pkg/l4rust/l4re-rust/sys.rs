#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use core::{
    convert::TryInto,
    ffi::{c_int, c_long, c_ulong, c_void},
    ptr::NonNull,
};
use l4::sys::helpers::eq_str_cstr;

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

pub use L4re_protocols as L4ReProtocols;
////////////////////////////////////////////////////////////////////////////////
// Custom types

////////////////////////////////////////////////////////////////////////////////
// re-implementations of inlined C functions
#[inline]
pub unsafe fn l4re_env() -> *const l4re_env_t {
    l4re_global_env
}

/// Get the capability (selector) from the environment with the specified name
///
/// Each appplication comes with a set of capabilities predefined by its
/// environment (parent). This function can query for the capability selector
/// by the given name, returning None if no such  name was found.
/// It is advised to use l4re::env::get_cap instead.
#[inline]
pub fn l4re_env_get_cap(name: &str) -> Option<l4_cap_idx_t> {
    // SAFETY: the unsafety stems from using pointers that are by the function signature declared
    // to be non-null
    unsafe {
        l4re_env_get_cap_l(name, NonNull::new(l4re_env() as *mut l4re_env_t))
            .map(|record| record.as_ref().cap)
    }
}

#[inline]
unsafe fn l4re_env_get_cap_l(
    name: &str,
    env: Option<NonNull<l4re_env_t>>,
) -> Option<NonNull<l4re_env_cap_entry_t>> {
    let mut c = unsafe { env?.as_ref() }.caps;
    while !c.is_null() && (*c).flags != !0u64 {
        if eq_str_cstr(name, ((&(*c).name) as *const _) as *const u8) {
            return NonNull::new(c);
        }
        c = c.offset(1); // advance one record
    }
    None
}

/// Attach given address space to task-global region map.
///
/// This function uses the L4::Env::env()->rm() service.
#[inline]
pub unsafe fn l4re_rm_attach(
    start: *mut *mut c_void,
    size: l4_addr_t,
    flags: u64,
    mem: l4re_ds_t,
    offs: l4_addr_t,
    align: u8,
) -> i32 {
    l4re_rm_attach_srv(
        (*l4re_global_env).rm,
        start,
        size as u64,
        flags.try_into().unwrap(),
        mem,
        offs,
        align,
    )
}

#[inline]
pub unsafe fn l4re_rm_detach(addr: *mut c_void) -> c_int {
    l4re_rm_detach_w(addr)
}

#[inline]
pub unsafe fn l4re_rm_detach_ds(addr: *mut c_void, ds: *mut l4re_ds_t) -> i32 {
    l4re_rm_detach_ds_w(addr, ds)
    //l4re_rm_detach_srv((*l4re_global_env).rm, addr as l4_addr_t,
    //        ds, L4_BASE_TASK_CAP as u64)
}

#[inline]
pub unsafe fn l4re_ma_alloc(size: usize, mem: l4re_ds_t, flags: c_ulong) -> c_long {
    l4re_ma_alloc_w(size as i64, mem, flags)
}
