#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
// ToDo: get rid of these
#![feature(pointer_methods)]

#![feature(asm)]


include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

////////////////////////////////////////////////////////////////////////////////
// inline functions from l4/sys/ipc.h:

#[inline]
pub unsafe fn l4_utcb_mr() -> *mut l4_msg_regs_t {
    l4_utcb_mr_w()
}

#[inline]
pub unsafe fn l4_ipc_call(dest: l4_cap_idx_t, utcb: *mut l4_utcb_t,
            tag: l4_msgtag_t, timeout: l4_timeout_t) -> l4_msgtag_t {
    l4_ipc_call_w(dest, utcb, tag, timeout)
}

#[inline]
pub unsafe fn l4_ipc_error(tag: l4_msgtag_t, utcb: *mut l4_utcb_t) -> l4_umword_t {
    l4_ipc_error_w(tag, utcb)
}

#[inline]
pub unsafe fn l4_ipc_wait(utcb: *mut l4_utcb_t, label: *mut l4_umword_t,
        timeout: l4_timeout_t) -> l4_msgtag_t {
    l4_ipc_wait_w(utcb, label, timeout)
}

#[inline]
pub unsafe fn l4_utcb() -> *mut l4_utcb_t {
    l4_utcb_w()
}

type unsigned = ::std::os::raw::c_uint;

#[inline]
pub unsafe fn l4_msgtag(label: ::std::os::raw::c_long, words: unsigned,
        items: unsigned, flags: unsigned) -> l4_msgtag_t {
    l4_msgtag_w(label, words, items, flags)
}

#[inline]
pub unsafe fn l4_ipc_reply_and_wait(utcb: *mut l4_utcb_t, tag: l4_msgtag_t,
        src: *mut l4_umword_t, timeout: l4_timeout_t) -> l4_msgtag_t {
    l4_ipc_reply_and_wait_w(utcb, tag, src, timeout)
}
