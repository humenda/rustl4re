#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
// ToDo: get rid of these
#![feature(pointer_methods)]

#![feature(asm)]

use std::{ffi::CString,
        os::raw::{c_uchar, c_uint}};

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

////////////////////////////////////////////////////////////////////////////////
// inline functions from l4/sys/env.h:


#[inline]
pub unsafe fn l4re_env_get_cap(name: &str) -> l4_cap_idx_t {
    let name = CString::from_vec_unchecked(name.as_bytes().to_vec());
    l4re_env_get_cap_w(name.as_ptr())
}

#[inline]
pub fn l4_is_invalid_cap(cap: l4_cap_idx_t) -> bool {
    (cap & l4_cap_consts_t_L4_INVALID_CAP_BIT) > 0
}

#[inline]
pub unsafe fn l4_obj_fpage(obj: l4_cap_idx_t, order: c_uint, rights: c_uchar)
        -> l4_fpage_t
{
    l4_obj_fpage_w(obj, order, rights)
}
