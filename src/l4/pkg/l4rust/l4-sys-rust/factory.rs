//! Low-level factory interface to create kernel objects.
//!
//! A factory is used to create all kinds of kernel objects: tasks, threads, IPC gates, factories,
//! IRQ and VM.
//! To create a new kernel object the caller has to specify the factory to use for creation. The
//! caller has to allocate a capability slot where the kernel stores the new object's capability.
//!
//! The factory is equipped with a limit that limits the amount of kernel memory available for that
//! factory.
//!
//! **Note:** The limit does not give any guarantee for the amount of available kernel memory.

use libc::{l4_umword_t, l4_mword_t};

use core::ptr;
use core::mem::size_of;

use crate::c_api::{*, l4_msgtag_protocol as MsgTagProto};
use crate::cap;
use crate::consts::UtcbConsts;
use crate::ipc_basic::{l4_ipc_call, l4_utcb, l4_utcb_mr_u, l4_utcb_br_u, timeout_never};
use crate::ipc_ext::{msgtag, msgtag_flags, msgtag_label, msgtag_words};
use crate::helpers;

/// Create a new task.
///
/// **Note:** The size of the UTCB area specifies indirectly the number of UTCBs available for this
/// task. Refer to `l4_add_ku_mem`.
/// The new capability is stored in `target_cap`.
#[inline]
pub unsafe fn l4_factory_create_task(factory: l4_cap_idx_t,
        target_cap: &mut l4_cap_idx_t, utcb_area: l4_fpage_t) -> l4_msgtag_t {
    l4_factory_create_task_u(factory, target_cap, utcb_area, l4_utcb())
}

#[inline]
pub unsafe fn l4_factory_create_task_u(factory: l4_cap_idx_t,
        target_cap: &mut l4_cap_idx_t, utcb_area: l4_fpage_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    let mut tag = l4_factory_create_start_u(
            MsgTagProto::L4_PROTO_TASK as i32,
                                            *target_cap, u);
    l4_factory_create_add_fpage_u(utcb_area, &mut tag, u);
    l4_factory_create_commit_u(factory, tag, u)
}

/// Create a new thread
///
/// Create a new thread with the given capability. The capability has to be allocated before.
#[inline]
pub fn l4_factory_create_thread(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t) -> l4_msgtag_t {
    // This is safe because most of the functions called from that one below are safe and the only
    // one which could fail is the IPC call itself if the UTCB were a null pointer, but we give it
    // a valid pointer below
    unsafe {
        l4_factory_create_thread_u(factory, target_cap, l4_utcb())
    }
}

#[inline]
pub unsafe fn l4_factory_create_thread_u(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t, u: *mut l4_utcb_t) -> l4_msgtag_t {
    l4_factory_create_u(factory, MsgTagProto::L4_PROTO_THREAD as i32,
                        target_cap, u)
}

/// Create a factory
///
/// This creates a new factory with the given `target_cap`, which needs to be allocated before.
#[inline]
pub unsafe fn l4_factory_create_factory(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t, limit: l4_umword_t) -> l4_msgtag_t {
    l4_factory_create_factory_u(factory, target_cap, limit, l4_utcb())
}

#[inline]
pub unsafe fn l4_factory_create_factory_u(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t, limit: l4_umword_t, u: *mut l4_utcb_t)
        -> l4_msgtag_t {
    let mut tag = l4_factory_create_start_u(
            MsgTagProto::L4_PROTO_FACTORY as i32, target_cap, u);
    l4_factory_create_add_uint_u(limit, &mut tag, u);
    l4_factory_create_commit_u(factory, tag, u)
}

/// Create a new IPC gate
///
/// This factory function creates a new IPC gate at the capability index given by `target_cap`
/// (which needs to be allocated before). If `thread_cap` is not `L4_INVALID_CAP`, the given thread
/// is bound to the newly created gate, otherwise it is left unbound.
/// An unbound IPC gate can be bound later to a thread using `l4_ipc_gate_bind_thread`.
///
/// Possible message tag error codes include:
///
/// - `L4_EOK`:      No error occurred.
/// -    `-L4_ENOMEM`:  Out-of-memory during allocation of the Ipc_gate object.
/// - `-L4_ENOENT`:  `thread_cap` is void or points to something that is not a thread.
/// - `-L4_EPERM`:  No write rights on `thread_cap`.
#[inline]
pub unsafe fn l4_factory_create_gate(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t, thread_cap: l4_cap_idx_t, label: l4_umword_t)
        -> l4_msgtag_t {
    l4_factory_create_gate_u(factory, target_cap, thread_cap, label, l4_utcb())
}

#[inline]
pub unsafe fn l4_factory_create_gate_u(factory: l4_cap_idx_t,
        target_cap: l4_cap_idx_t, thread_cap: l4_cap_idx_t, label: l4_umword_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    let mut items = 0;
    let mut tag = l4_factory_create_start_u(0, target_cap, u);
    l4_factory_create_add_uint_u(label, &mut tag, u);
    let v = l4_utcb_mr_u(u);
    if (thread_cap & l4_cap_consts_t::L4_INVALID_CAP_BIT as l4_umword_t) == 0 {
        items = 1;
        mr!(v[3] = cap::l4_map_obj_control(0,0));
        mr!(v[4] = cap::l4_obj_fpage(thread_cap, 0,
                                     L4_fpage_rights::L4_FPAGE_RWX as u8)
            .raw);
    }
    tag = msgtag(msgtag_label(tag), msgtag_words(tag), items,
            msgtag_flags(tag));
    l4_factory_create_commit_u(factory, tag, u)
}

// ToDo: unported
///**
// * \ingroup l4_factory_api
// * Create a new IRQ sender.
// *
// * \param      factory     Factory to use for creation.
// * \param[out] target_cap  The kernel stores the new IRQ's capability into this
// *                         slot.
// *
// * \return Syscall return tag
// *
// * \see \ref l4_irq_api
// */
//L4_INLINE l4_msgtag_t
//l4_factory_create_irq(l4_cap_idx_t factory,
//                      l4_cap_idx_t target_cap) L4_NOTHROW;
//
///**
// * \ingroup l4_factory_api
// * \copybrief L4::Factory::create_irq
// * \param factory  Factory to use for creation.
// * \copydetails L4::Factory::create_irq
// */
//L4_INLINE l4_msgtag_t
//l4_factory_create_irq_u(l4_cap_idx_t factory,
//                        l4_cap_idx_t target_cap, l4_utcb_t *utcb) L4_NOTHROW;
//
///**
// * \ingroup l4_factory_api
// * \copybrief L4::Factory::create_vm
// * \param      factory     Capability selector for factory to use for creation.
// * \param[out] target_cap  The kernel stores the new VM's capability into this
// *                         slot.
// *
// * \return Syscall return tag
// *
// * \see \ref l4_vm_api
// */
//L4_INLINE l4_msgtag_t
//l4_factory_create_vm(l4_cap_idx_t factory,
//                     l4_cap_idx_t target_cap) L4_NOTHROW;
//
///**
// * \ingroup l4_factory_api
// * \copybrief L4::Factory::create_vm
// * \param factory  Capability selector for factory to use for creation.
// * \copydetails L4::Factory::create_vm
// */
//L4_INLINE l4_msgtag_t
//l4_factory_create_vm_u(l4_cap_idx_t factory,
//                       l4_cap_idx_t target_cap, l4_utcb_t *utcb) L4_NOTHROW;
//
//L4_INLINE l4_msgtag_t
// ToDo: docs
#[inline]
pub fn l4_factory_create_add_int_u(d: l4_mword_t, tag: &mut l4_msgtag_t,
        u: *mut l4_utcb_t) -> bool {
    // safe, because tag is a mutable reference and hence never null and l4_utcb_mr_u works
    // *always*
    let mut w = msgtag_words(*tag) as usize;
    if w + 2 > UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize {
        return false;
    }
    unsafe {
        let v = l4_utcb_mr_u(u);
        mr!(v[w] = L4_varg_type::L4_VARG_TYPE_MWORD as l4_umword_t
                | (size_of::<l4_mword_t>() << 16) as l4_umword_t);
        mr!(v[w + 1] = d);
    }
    w += 2;
    tag.raw = (tag.raw & !(0x3f as l4_mword_t)) | (w & 0x3f) as l4_mword_t;
    true
}

#[inline]
pub fn l4_factory_create_add_uint_u(d: l4_umword_t, tag: &mut l4_msgtag_t,
        u: *mut l4_utcb_t) -> bool {
    let mut w = msgtag_words(*tag);
    if w + 2 > UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as u32 {
        return false;
    }
    unsafe {
        let v = l4_utcb_mr_u(u);
        mr!(v[w] = L4_varg_type::L4_VARG_TYPE_UMWORD as l4_umword_t
                | (size_of::<l4_umword_t>() << 16) as l4_umword_t);
        mr!(v[w + 1] = d);
    }
    w += 2;
    tag.raw = (tag.raw & !(0x3f as l4_mword_t)) | (w as l4_mword_t & (0x3f as l4_mword_t));
    true
}


#[inline]
pub unsafe fn l4_factory_create_add_cstr_u(s: *const u8,
        tag: &mut l4_msgtag_t, u: *mut l4_utcb_t) -> bool {
    let mut w = msgtag_words(*tag) as usize;
    let len = helpers::strlen(s) as usize;
    if w + 1 + (len + size_of::<l4_umword_t>() - 1) / size_of::<l4_umword_t>()
            > (UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize) {
        return false;
    }
    let v = l4_utcb_mr_u(u);
    mr!(v[w] = L4_varg_type::L4_VARG_TYPE_STRING as usize | (len << 16));
    let c = &mut (*v).mr.as_mut()[w as usize + 1] as *mut l4_umword_t as *mut u8;
    ptr::copy_nonoverlapping(s, c, len + 1);
    w = w + 1 + (len + size_of::<l4_umword_t>() - 1) / size_of::<l4_umword_t>();
    tag.raw = (tag.raw & !(0x3f as l4_mword_t)) | (w as l4_mword_t & (0x3f as l4_mword_t));
    true
}

#[inline]
pub fn l4_factory_create_add_str_u(s: &str, tag: &mut l4_msgtag_t,
        u: *mut l4_utcb_t) -> bool {
    let mut w = msgtag_words(*tag);
    let len = s.len();
    if w as usize + 1 + (len + size_of::<l4_umword_t>() - 1) / size_of::<l4_umword_t>()
            > UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize {
        return false;
    }
    unsafe {
        let v = l4_utcb_mr_u(u);
        mr!(v[w] = L4_varg_type::L4_VARG_TYPE_STRING as usize | (len << 16));
        let c = &mut (*v).mr.as_mut()[w as usize + 1] as *mut l4_umword_t as *mut u8;
        ptr::copy_nonoverlapping(s.as_bytes().as_ptr(), c, len);
        let c = ((c as usize) + len) as *mut u8;
        *c = 0; // add 0-byte
    }
    w = w + 1 + ((len + size_of::<l4_umword_t>() - 1) /
                 size_of::<l4_umword_t>()) as u32;
    tag.raw = (tag.raw & !0x3f) | (w  as l4_mword_t & 0x3f);
    true
}

#[inline]
pub unsafe fn l4_factory_create_commit_u(factory: l4_cap_idx_t,
        tag: l4_msgtag_t, u: *mut l4_utcb_t) -> l4_msgtag_t {
    l4_ipc_call(factory, u, tag, timeout_never())
}

#[inline]
pub unsafe fn l4_factory_create_u(factory: l4_cap_idx_t, protocol: i32,
        target: l4_cap_idx_t, utcb: *mut l4_utcb_t) -> l4_msgtag_t {
    let t = l4_factory_create_start_u(protocol, target, utcb);
    l4_factory_create_commit_u(factory, t, utcb)
}



#[inline]
pub unsafe fn l4_factory_create(factory: l4_cap_idx_t, protocol: i32,
        target: l4_cap_idx_t) -> l4_msgtag_t {
    l4_factory_create_u(factory, protocol, target, l4_utcb())
}

// ToDo: unported
//L4_INLINE l4_msgtag_t
//l4_factory_create_irq_u(l4_cap_idx_t factory,
//                        l4_cap_idx_t target_cap, l4_utcb_t *u) L4_NOTHROW
//{
//  return l4_factory_create_u(factory, L4_PROTO_IRQ_SENDER, target_cap, u);
//}
//
//L4_INLINE l4_msgtag_t
//l4_factory_create_vm_u(l4_cap_idx_t factory,
//                       l4_cap_idx_t target_cap,
//                       l4_utcb_t *u) L4_NOTHROW
//{
//  return l4_factory_create_u(factory, L4_PROTO_VM, target_cap, u);
//}
//                       
//L4_INLINE l4_msgtag_t
//l4_factory_create_irq(l4_cap_idx_t factory,
//                      l4_cap_idx_t target_cap) L4_NOTHROW
//{
//  return l4_factory_create_irq_u(factory, target_cap, l4_utcb());
//}
//
//L4_INLINE l4_msgtag_t
//l4_factory_create_vm(l4_cap_idx_t factory,
//                     l4_cap_idx_t target_cap) L4_NOTHROW
//{
//  return l4_factory_create_vm_u(factory, target_cap, l4_utcb());
//}

/// Prepare UTCB for receiving specified object
///
/// This function instructs the factory to allocate a new object. The type is determined by the
/// given protocol. See `MsgTagProto`.
#[inline]
unsafe fn l4_factory_create_start_u(protocol: i32, target_cap: l4_cap_idx_t,
                          u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    let b = l4_utcb_br_u(u);
    mr!(v[0] = protocol);
    (*b).bdr = 0;
    (*b).br[0] = target_cap | l4_msg_item_consts_t::L4_RCV_ITEM_SINGLE_CAP as l4_umword_t;
    msgtag(MsgTagProto::L4_PROTO_FACTORY as l4_mword_t, 1, 0, 0)
}

#[inline]
/// Add a flex page to the given UTCB
pub fn l4_factory_create_add_fpage_u(d: l4_fpage_t, tag: &mut l4_msgtag_t,
        u: *mut l4_utcb_t) -> bool {
    let mut w = msgtag_words(*tag);
    if w + 2 > UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as u32 {
        return false; 
    }
    unsafe {
        let v = l4_utcb_mr_u(u);
        mr!(v[w] = L4_varg_type::L4_VARG_TYPE_FPAGE as usize | (size_of::<l4_fpage_t>() << 16));
        mr!(v[w + 1] = d.raw);
    }
    w += 2;
    tag.raw = (tag.raw & !(0x3f as l4_mword_t)) | (w as l4_mword_t & 0x3f);
    true
}

#[inline]
pub unsafe fn l4_factory_create_add_nil_u(tag: &mut l4_msgtag_t,
        u: *mut l4_utcb_t) -> bool {
  let mut w = msgtag_words(*tag);
    if w + 1 > UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as u32 {
        return false; 
    }

    let v = l4_utcb_mr_u(u);
    mr!(v[w] = L4_varg_type::L4_VARG_TYPE_NIL);
    w += 1;
    tag.raw = (tag.raw & !(0x3f as l4_mword_t)) | (w  as l4_mword_t & 0x3f);
    true
}

