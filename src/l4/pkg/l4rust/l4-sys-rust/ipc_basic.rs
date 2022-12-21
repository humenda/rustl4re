use core::ffi::{c_int, c_long, c_uint, c_ulong};

use crate::c_api::{l4_error_code_t::*, l4_msg_item_consts_t::*, L4_fpage_control::*, *};
use crate::consts::*;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// simple wrappers from lib-l4re-rust-wrapper

#[must_use]
#[inline]
pub unsafe fn l4_ipc_call(
    dest: l4_cap_idx_t,
    utcb: *mut l4_utcb_t,
    tag: l4_msgtag_t,
    timeout: l4_timeout_t,
) -> l4_msgtag_t {
    l4_ipc_call_w(dest, utcb, tag, timeout)
}

#[must_use]
#[inline(always)]
pub unsafe fn l4_ipc_send(
    dest: l4_cap_idx_t,
    utcb: *mut l4_utcb_t,
    tag: l4_msgtag_t,
    timeout: l4_timeout_t,
) -> l4_msgtag_t {
    l4_ipc_send_w(dest, utcb, tag, timeout)
}

#[inline]
pub unsafe fn l4_ipc_error(tag: l4_msgtag_t, utcb: *mut l4_utcb_t) -> l4_umword_t {
    l4_ipc_error_w(tag, utcb)
}

#[must_use]
#[inline]
pub unsafe fn l4_ipc_receive(
    object: l4_cap_idx_t,
    utcb: *mut l4_utcb_t,
    timeout: l4_timeout_t,
) -> l4_msgtag_t {
    l4_ipc_receive_w(object, utcb, timeout)
}

#[must_use]
#[inline]
pub unsafe fn l4_ipc_reply_and_wait(
    utcb: *mut l4_utcb_t,
    tag: l4_msgtag_t,
    src: *mut l4_umword_t,
    timeout: l4_timeout_t,
) -> l4_msgtag_t {
    l4_ipc_reply_and_wait_w(utcb, tag, src, timeout)
}

#[must_use]
#[inline]
pub unsafe fn l4_ipc_wait(
    utcb: *mut l4_utcb_t,
    label: *mut l4_umword_t,
    timeout: l4_timeout_t,
) -> l4_msgtag_t {
    l4_ipc_wait_w(utcb, label, timeout)
}

/// This function only serves compatibility. One can use the MsgTag struct from the l4 crate
/// instead.
#[inline]
pub fn l4_msgtag(label: c_long, words: c_uint, items: c_uint, flags: c_uint) -> l4_msgtag_t {
    l4_msgtag_t {
        raw: (label << 16)
            | (words & 0x3f) as l4_mword_t
            | ((items & 0x3f) << 6) as l4_mword_t
            | (flags & 0xf000) as l4_mword_t,
    }
}

#[must_use]
#[inline]
pub unsafe fn l4_rcv_ep_bind_thread(
    gate: l4_cap_idx_t,
    thread: l4_cap_idx_t,
    label: l4_umword_t,
) -> l4_msgtag_t {
    l4_rcv_ep_bind_thread_w(gate, thread, label)
}

////////////////////////////////////////////////////////////////////////////////
// re-implemented inline functions from l4/sys/ipc.h:

#[inline]
pub unsafe fn l4_msgtag_flags(t: l4_msgtag_t) -> c_ulong {
    (t.raw & 0xf000) as c_ulong
}

/// Re-implementation returning bool instead of int
#[inline]
pub fn l4_msgtag_has_error(t: l4_msgtag_t) -> bool {
    (t.raw & MSGTAG_ERROR) != 0
}

/// This function's return type is altered in comparison to the orig: c_uint -> usize, return value
/// is used as index and this needs to be usize in Rust world.
#[inline]
pub fn l4_msgtag_items(t: l4_msgtag_t) -> usize {
    ((t.raw >> 6) & 0x3f) as usize
}

#[inline]
pub fn l4_msgtag_label(t: l4_msgtag_t) -> c_long {
    t.raw >> 16
}

#[inline]
pub fn l4_msgtag_words(t: l4_msgtag_t) -> u32 {
    (t.raw & 0x3f) as u32
}

/// See sndfpage_add_u and sndfpage_add
#[inline]
pub unsafe fn l4_sndfpage_add_u(
    snd_fpage: l4_fpage_t,
    snd_base: c_ulong,
    tag: *mut l4_msgtag_t,
    utcb: *mut l4_utcb_t,
) -> c_int {
    let i = l4_msgtag_words(*tag) as usize + 2 * l4_msgtag_items(*tag);
    if i >= (UTCB_GENERIC_DATA_SIZE - 1) {
        return L4_ENOMEM as i32 * -1;
    }

    let v = l4_utcb_mr_u(utcb);
    mr!(v[i] = snd_base | L4_ITEM_MAP as u64 | L4_ITEM_CONT as u64);
    mr!(v[i + 1] = snd_fpage.raw);

    *tag = l4_msgtag(
        l4_msgtag_label(*tag),
        l4_msgtag_words(*tag),
        l4_msgtag_items(*tag) as u32 + 1,
        l4_msgtag_flags(*tag) as u32,
    );
    0
}

#[inline]
pub unsafe fn l4_utcb_mr() -> *mut l4_msg_regs_t {
    l4_utcb_mr_u(l4_utcb())
}

/// re-implementation of the inline function from l4sys/include/ipc.h
#[inline]
pub unsafe fn l4_sndfpage_add(
    snd_fpage: l4_fpage_t,
    snd_base: c_ulong,
    tag: *mut l4_msgtag_t,
) -> c_int {
    l4_sndfpage_add_u(snd_fpage, snd_base, tag, l4_utcb())
}

#[inline(always)]
pub unsafe fn l4_utcb() -> *mut l4_utcb_t {
    l4_utcb_w()
}

// ToDo: potentially broken (or one of the functions called
//#[inline]
//pub unsafe fn l4_rcv_ep_bind_thread(gate: l4_cap_idx_t, thread: l4_cap_idx_t,
//        label: l4_umword_t) -> l4_msgtag_t {
//    return l4_rcv_ep_bind_thread_u(gate, thread, label, l4_utcb())
//}

#[inline]
pub fn l4_map_control(snd_base: l4_umword_t, cache: u8, grant: u32) -> l4_umword_t {
    (snd_base & L4_FPAGE_CONTROL_MASK as l4_umword_t)
        | ((cache as l4_umword_t) << 4)
        | L4_ITEM_MAP as u64
        | (grant as l4_umword_t)
}

/// Retrieve pointer to buffer registers from current UTCB
#[inline(always)]
pub unsafe fn l4_utcb_br() -> *mut l4_buf_regs_t {
    (l4_utcb() as *mut u8).offset(UTCB_BUF_REGS_OFFSET) as *mut l4_buf_regs_t
}

/// Retrieve pointer to buffer register from specified UTCB
#[inline(always)]
pub unsafe fn l4_utcb_br_u(u: *mut l4_utcb_t) -> *mut l4_buf_regs_t {
    (u as *mut u8).offset(UTCB_BUF_REGS_OFFSET) as *mut l4_buf_regs_t
}

#[inline]
pub unsafe fn l4_utcb_mr_u(u: *mut l4_utcb_t) -> *mut l4_msg_regs_t {
    (u as *mut u8).offset(UtcbConsts::L4_UTCB_MSG_REGS_OFFSET as isize) as *mut l4_msg_regs_t
}

////////////////////////////////////////////////////////////////////////////////
// new functions

/// Return a never-timeout (akin to L4_IPC_TIMEOUT_NEVER)
#[inline(always)]
pub fn timeout_never() -> l4_timeout_t {
    l4_timeout_t { raw: 0 }
}

/// Extract IPC error code from error code
///
/// Error codes in the console output, e.g. for  page faults, contain more information than the IPC
/// error code, this function extracts this bit of information.
#[inline(always)]
pub fn ipc_error2code(code: isize) -> u64 {
    (code & l4_ipc_tcr_error_t::L4_IPC_ERROR_MASK as isize) as u64
}

/// Default protocol used by IPC interfaces
pub const PROTO_ANY: i64 = 0;
/// Empty protocol for empty APIs
pub const PROTO_EMPTY: i64 = -19;
