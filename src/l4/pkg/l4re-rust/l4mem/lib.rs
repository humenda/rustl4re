#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use std::os::raw::{c_long, c_ulong, c_void};

// it's mostly used as "flags" parameter, wich is u8; therefore redefine what bindgen would have
// defined before
// arch-dependent define (l4sys/include/ARCH-amd64/consts.h)
#[cfg(target_arch = "x86_64")]
pub const L4_PAGESHIFT: u8 = 12;
#[cfg(target_arch = "x86_64")]
pub const L4_PAGEMASKU: u64 = L4_PAGEMASK as u64;
pub const L4_PAGEMASKUSIZE: usize = L4_PAGEMASK as usize;
pub const L4_PAGESIZEU: usize = L4_PAGESIZE as usize;


////////////////////////////////////////////////////////////////////////////////
// re-implementations of inlined C functions
#[inline]
pub fn l4_trunc_page(address: l4_addr_t) -> l4_addr_t {
    address & L4_PAGEMASKU
}

/// the function above makes interfacing with l4 C code easy, that one below is more appropriate
/// (and potentially more efficient) for Rust code
#[inline]
fn l4_trunc_page_u(address: usize) -> usize {
    address & L4_PAGEMASKUSIZE
}

/// Attach given address space to task-global region map.
///
/// This function uses the L4::Env::env()->rm() service.
#[inline]
pub unsafe fn l4re_rm_attach(start: *mut *mut c_void, size: u64, flags: u64,
        mem: l4re_ds_t, offs: l4_addr_t, align: u8) -> i32 {
    l4re_rm_attach_srv((*l4re_global_env).rm, start, size, flags, mem, offs,
            align)
}

#[inline]
pub unsafe fn l4re_rm_detach_ds(addr: *mut c_void, ds: *mut l4re_ds_t) -> i32 {
    l4re_rm_detach_ds_w(addr, ds)
    //l4re_rm_detach_srv((*l4re_global_env).rm, addr as l4_addr_t,
    //        ds, L4_BASE_TASK_CAP as u64)
}

#[inline]
pub unsafe fn l4re_ma_alloc(size: usize, mem: l4re_ds_t, flags: c_ulong)
        -> c_long {
    l4re_ma_alloc_w(size as i64, mem, flags)
}


////////////////////////////////////////////////////////////////////////////////
// convenience functions
pub fn required_shared_mem<T>(thing_to_share: &T) -> usize {
    let size_obj = ::std::mem::size_of_val(thing_to_share);
    return required_pages(size_obj)
}

/// Round given value to a full L4_PAGESIZE
#[inline]
pub fn required_pages(bytes: usize) -> usize {
    l4_trunc_page_u(bytes) + match bytes % L4_PAGESIZEU {
        0 => 0,
        _ => L4_PAGESIZEU
    }
}

