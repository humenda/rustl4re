#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
// ToDo: get rid of these
#![feature(pointer_methods)]

#![feature(asm)]


include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

////////////////////////////////////////////////////////////////////////////////
// inline functions from l4/sys/ipc.h:

///// retrieve base pointer to UTCB, saved in GS segment register for amd64
//#[cfg(target_arch = "x86_64")]
//#[inline]
//pub unsafe fn l4_utcb_direct() -> *mut l4_utcb_t {
//    let res: *mut l4_utcb_t;
//    asm!("mov %gs:0, $0\n" : "=r"(res));
//    res
//}
//
//#[inline]
//pub unsafe fn  l4_utcb() -> *mut l4_utcb_t {
//    // this is not ported yet: #ifdef L4SYS_USE_UTCB_WRAP
//    // return l4_utcb_wrap();
//    // ported:  #else
//    l4_utcb_direct()
//}
//
//pub unsafe fn l4_utcb_mr_u(u: *mut l4_utcb_t) -> *mut l4_msg_regs_t {
//    // ToDo: the C source code adds L4_UTCB_MSG_REGS_OFFSET, but that's 0, so it boils down to a cast, use enum
//    transmute(u)
//}
//
//#[inline]
//pub unsafe fn l4_utcb_mr() -> *mut l4_msg_regs_t {
//    l4_utcb_mr_u(l4_utcb())
//}
//
//// ToDo: these below are already verfiied for --cfg, remove this notice as soon as those above have
//// been reworked, too
//#[inline]
//pub unsafe fn l4_ipc_call(dest: l4_cap_idx_t, utcb: *mut l4_utcb_t,
//            tag: l4_msgtag_t, timeout: l4_timeout_t) -> l4_msgtag_t {
//    // tOdO:l4_syscall_flags_tL4_SYSF_CALL is actually l4_syscall_flags_t::L4_SYSF_CALL , why enum
//    // destroyed?
//    return l4_ipc(dest, utcb,
//                  l4_syscall_flags_t_L4_SYSF_CALL as u64, // ToDo: why umword_t, but pass enum in which is u32
//                  0, tag, None, timeout);
//}
//
//
/////// literal syscall from l4sys/include/ARCH-amd64/L4API-l4f/ipc.h
////#[inline]
////#[cfg(target_arch = "x86_64")]
////// ToDo: Carsten, warum utcb ungenutzt
////pub unsafe fn l4_ipc(dest: l4_cap_idx_t, utcb: *mut l4_utcb_t,
////       flags: l4_umword_t, mut slabel: l4_umword_t,
////       mut tag: l4_msgtag_t, rlabel: Option<*mut l4_umword_t>,
////       timeout: l4_timeout_t) -> l4_msgtag_t {
////    let _dummy: l4_umword_t;
////    let _dummy2: l4_umword_t;
////    // ToDo: register l4_umword_t to __asm__("r8") = timeout.raw;
////    asm!("mov $0 %r8" :: "r"(timeout.raw) : "r8");
////    
////    asm!("syscall"
////     :
////     "=d" (_dummy2),
////     "=S" (slabel),
////     "=D" (_dummy),
////     "=a" (tag.raw)
////     :
////     "S" (slabel),
////     "a" (tag.raw),
////     "d" (dest | flags)
////     :
////     "memory", "cc", "rcx", "r11", "r15"
////     : "volatile"
////     );
////
////  // ToDo: what is it, nicer
////    if let Some(rlabel) = rlabel {
////        *rlabel = slabel;
////    }
////    return tag;
////}
//
//#[inline]
//pub fn l4_msgtag_has_error(t: l4_msgtag_t) -> bool {
//    t.raw & (l4_msgtag_flags_L4_MSGTAG_ERROR as i64) != 0
//}
//#[inline]
//pub unsafe fn l4_ipc_error(tag: l4_msgtag_t, utcb: *mut l4_utcb_t) -> l4_umword_t {
//    match l4_msgtag_has_error(tag) {
//        false => 0,
//        true => (*l4_utcb_tcr_u(utcb)).error & l4_ipc_tcr_error_t_L4_IPC_ERROR_MASK as u64,
//    }
//}
//
//#[inline]
//pub unsafe fn l4_utcb_tcr_u(u: *mut l4_utcb_t) -> *mut l4_thread_regs_t {
//    (u as *mut u8).add(L4_utcb_consts_amd64_L4_UTCB_THREAD_REGS_OFFSET as usize) 
//        as *mut l4_thread_regs_t
//}
//
//
//// Message tag functions
//
//// ToDo: use libc
//type unsigned = u32;
//
///// Create a message tag from the specified values.
///// label:  The user-defined label
///// words:  The number of untyped words within the UTCB
///// items:  The number of typed items (e.g., flex pages) within the UTCB
///// flags:  The IPC flags for realtime and FPU extensions
///// returns: Message tag
//#[inline]
//pub fn l4_msgtag(label: ::std::os::raw::c_long, words: unsigned,
//    items: unsigned, flags: unsigned) -> l4_msgtag_t {
//    l4_msgtag_t { raw: (label << 16) | ((words & 0x3f) as l4_mword_t)
//            | (((items & 0x3f) << 6) as l4_mword_t)
//                       | ((flags & 0xf000) as l4_mword_t)
//    }
//}

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
