#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![no_std]

extern crate libc;

use libc::{c_int, c_long, c_ulong, c_void};

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

////////////////////////////////////////////////////////////////////////////////
// Custom types

////////////////////////////////////////////////////////////////////////////////
// redefined constants
// it's mostly used as "flags" parameter, wich is u8; therefore redefine what bindgen would have
// defined before
// arch-dependent define (l4sys/include/ARCH-amd64/consts.h)
#[cfg(target_arch = "x86_64")]
#[cfg(target_arch = "x86_64")]
pub const L4_PAGEMASKU: l4_addr_t = L4_PAGEMASK as l4_addr_t;
pub const L4_PAGEMASKUSIZE: usize = L4_PAGEMASK as usize;
pub const L4_PAGESIZEU: usize = L4_PAGESIZE as usize;


////////////////////////////////////////////////////////////////////////////////
// re-implementations of inlined C functions
#[inline]
pub unsafe fn l4re_env() -> *const l4re_env_t {
    l4re_global_env
}

#[inline(always)]
//fn l4re_env() -> Result<&'static l4re_env_t> {
//    unsafe {
//        if l4re_global_env.is_null() {
//            return Err(Error::InvalidArg("Unable to get L4Re environment pointer, \
//                not set up (either no l4re binary or no libc in use)"));
//        }
//        Ok(::core::mem::transmute::<*const l4re_env_t, &'static l4re_env_t>(
//                l4re_global_env))
//    }
//    }


#[must_use]
#[inline]
pub unsafe fn l4re_env_get_cap(name: &str) -> l4_cap_idx_t {
    l4re_env_get_cap_w(name.as_ptr())
}

#[inline]
pub fn l4_trunc_page(address: l4_addr_t) -> l4_addr_t {
    address & L4_PAGEMASKU
}

/// Round address up to the next page.
///
/// The given address is rounded up to the next minimal page boundary. On most architectures this is a 4k
/// page. Check `L4_PAGESIZE` for the minimal page size.
#[inline]
//#[cfg(not(target_arch = "x86_64"))]
pub fn l4_round_page(address: usize) -> l4_addr_t {
    ((address + L4_PAGESIZE as usize - 1usize) & (L4_PAGEMASK as usize)) as l4_addr_t
}

/// the function above makes interfacing with l4 C code easy, that one below is more appropriate
/// (and potentially more efficient) for Rust code
#[inline]
pub fn l4_trunc_page_u(address: usize) -> usize {
    address & L4_PAGEMASKUSIZE
}

/// Attach given address space to task-global region map.
///
/// This function uses the L4::Env::env()->rm() service.
#[inline]
pub unsafe fn l4re_rm_attach(start: *mut *mut c_void, size: l4_addr_t, flags: u64,
        mem: l4re_ds_t, offs: l4_addr_t, align: u8) -> i32 {
    l4re_rm_attach_srv((*l4re_global_env).rm, start, size as u64, flags, mem, offs,
            align)
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
pub unsafe fn l4re_ma_alloc(size: usize, mem: l4re_ds_t, flags: c_ulong)
        -> c_long {
    l4re_ma_alloc_w(size as i64, mem, flags)
}


