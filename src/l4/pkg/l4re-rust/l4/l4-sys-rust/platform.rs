//! interface for controlling platform-wide properties.
//!
//!  The API allows a client to suspend, reboot or shutdown the system.

use c_api::*;
use ipc_basic::{l4_ipc_call, l4_utcb, l4_utcb_mr_u, timeout_never};
use ipc_ext::msgtag;

/// Suspend
pub const L4_PLATFORM_CTL_SYS_SUSPEND_OP: u64  = 0;
/// shutdown/reboot
pub const L4_PLATFORM_CTL_SYS_SHUTDOWN_OP: u64 = 1;
/// enable an offline CPU
pub const L4_PLATFORM_CTL_CPU_ENABLE_OP: u64   = 3;
/// disable an online CPU
pub const L4_PLATFORM_CTL_CPU_DISABLE_OP: u64  = 4;
/// Protocol messages to a platform control object.
pub const L4_PROTO_PLATFORM_CTL: i64 = 0;

/// Enter suspend to RAM.
///
/// The `extras` options are undocumented in the original documentation.
#[inline]
pub fn l4_platform_ctl_system_suspend(pfc: l4_cap_idx_t,
        extras: l4_umword_t)  -> l4_msgtag_t {
    unsafe { // the only unsafe bit is the l4_utcb() which is passed safely
        l4_platform_ctl_system_suspend_u(pfc, extras, l4_utcb())
    }
}

#[inline]
pub unsafe fn l4_platform_ctl_system_suspend_u(pfc: l4_cap_idx_t,
        extras: l4_umword_t, utcb: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(utcb);
    (*v).mr[0] = L4_PLATFORM_CTL_SYS_SUSPEND_OP;
    (*v).mr[1] = extras;
    l4_ipc_call(pfc, utcb, msgtag(L4_PROTO_PLATFORM_CTL, 2, 0, 0),
                                          timeout_never())
}

/// Shutdown or reboot the system.
///
///If a valid platform control capability is given, the system is shut down when second parameter
///is set to 0 or rebootet if set to 1.
#[inline]
pub fn l4_platform_ctl_system_shutdown(pfc: l4_cap_idx_t,
        reboot: l4_umword_t) -> l4_msgtag_t {
    unsafe { // safe, because the only unsafe bit is the (here correctly) passed l4_utcb() pointer
        l4_platform_ctl_system_shutdown_u(pfc, reboot, l4_utcb())
    }
}

#[inline]
unsafe fn l4_platform_ctl_system_shutdown_u(pfc: l4_cap_idx_t,
        reboot: l4_umword_t, utcb: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(utcb);
    (*v).mr[0] = L4_PLATFORM_CTL_SYS_SHUTDOWN_OP as u64;
    (*v).mr[1] = reboot;
    l4_ipc_call(pfc, utcb, msgtag(L4_PROTO_PLATFORM_CTL, 2, 0, 0),
                timeout_never())
}

/// Enable an offline CPU.
///
/// Enable the given (currently offline) physical CPU core.
#[inline]
pub fn l4_platform_ctl_cpu_enable(pfc: l4_cap_idx_t,
        phys_id: l4_umword_t) -> l4_msgtag_t {
    unsafe {
        l4_platform_ctl_cpu_enable_u(pfc, phys_id, l4_utcb())
    }
}

#[inline]
pub unsafe fn l4_platform_ctl_cpu_enable_u(pfc: l4_cap_idx_t,
        phys_id: l4_umword_t, utcb: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(utcb);
    (*v).mr[0] = L4_PLATFORM_CTL_CPU_ENABLE_OP as u64;
    (*v).mr[1] = phys_id;
    l4_ipc_call(pfc, utcb, msgtag(L4_PROTO_PLATFORM_CTL, 2, 0, 0),
            timeout_never())
}

////////////////////////////////////////////////////////////////////////////////
// Unported

//
// * Disable an online CPU.
// *
// * \param pfc      Capability to the platform control object.
// * \param phys_id  Physical CPU id of CPU (e.g. local APIC id) to disable.
// *
// * \return System call message tag
// */
//L4_INLINE l4_msgtag_t
//l4_platform_ctl_cpu_disable(l4_cap_idx_t pfc,
//                            l4_umword_t phys_id) L4_NOTHROW;
//
//
// * \internal
// */
//L4_INLINE l4_msgtag_t
//l4_platform_ctl_cpu_disable_u(l4_cap_idx_t pfc,
//                              l4_umword_t phys_id,
//                              l4_utcb_t *utcb) L4_NOTHROW;
//
//
//
//
//
//L4_INLINE l4_msgtag_t
//l4_platform_ctl_cpu_disable_u(l4_cap_idx_t pfc,
//                              l4_umword_t phys_id,
//                              l4_utcb_t *utcb) L4_NOTHROW
//{
//  l4_msg_regs_t *v = l4_utcb_mr_u(utcb);
//  v->mr[0] = L4_PLATFORM_CTL_CPU_DISABLE_OP;
//  v->mr[1] = phys_id;
//  return l4_ipc_call(pfc, utcb, l4_msgtag(L4_PROTO_PLATFORM_CTL, 2, 0, 0),
//                                          L4_IPC_NEVER);
//}
//
//L4_INLINE l4_msgtag_t
//l4_platform_ctl_cpu_disable(l4_cap_idx_t pfc,
//                            l4_umword_t phys_id) L4_NOTHROW
//{
//  return l4_platform_ctl_cpu_disable_u(pfc, phys_id, l4_utcb());
//}
