//! Common task related definitions.
//!
//! A task represents a combination of the address spaces provided
//! by the L4Re micro kernel. A task consists of at least a memory address space
//! and an object address space. On IA32 there is also an IO-port address space.
//!
//! Task objects are created using the a factory to create new kernel objects.

use std::ptr;

use cap::{l4_obj_fpage, l4_map_obj_control};
use c_api::*;
use ipc_basic::{l4_ipc_call, l4_utcb, l4_utcb_mr_u, timeout_never};
use ipc_ext::msgtag;

/// Map resources available in the source task to a destination task.
///
/// This method allows for asynchronous rights delegation from one task to another. It can be used
/// to share memory as well as to delegate access to objects.
#[inline]
pub unsafe fn l4_task_map(dst_task: l4_cap_idx_t, src_task: l4_cap_idx_t,
            snd_fpage: l4_fpage_t, snd_base: l4_addr_t) -> l4_msgtag_t {
    l4_task_map_u(dst_task, src_task, snd_fpage, snd_base, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_map_u(dst_task: l4_cap_idx_t, src_task: l4_cap_idx_t,
        snd_fpage: l4_fpage_t, snd_base: l4_addr_t, u: *mut l4_utcb_t)
        -> l4_msgtag_t {
   let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_MAP_OP as u64;
    (*v).mr[3] = l4_map_obj_control(0,0);
    (*v).mr[4] = l4_obj_fpage(src_task, 0, L4_FPAGE_RWX as u8).raw as u64;
    (*v).mr[1] = snd_base;
    (*v).mr[2] = snd_fpage.raw;
    l4_ipc_call(dst_task, u,
            msgtag(L4_PROTO_TASK as i64, 3, 1, 0), timeout_never())
}

/// Revoke rights from the task.
///
/// This method allows to revoke rights from the destination task and from all
/// the tasks that got the rights delegated from that task (i.e., this operation
/// does a recursive rights revocation).
///
/// **Note:** Calling this function on the object space can cause a root
/// capability of an object to be destructed, which destroys the object itself.
#[inline]
pub unsafe fn l4_task_unmap(task: l4_cap_idx_t, fpage: l4_fpage_t,
        map_mask: l4_umword_t) -> l4_msgtag_t {
    l4_task_unmap_u(task, fpage, map_mask, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_unmap_u(task: l4_cap_idx_t, fpage: l4_fpage_t,
        map_mask: l4_umword_t, u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_UNMAP_OP as u64;
    (*v).mr[1] = map_mask as u64;
    (*v).mr[2] = fpage.raw;
    l4_ipc_call(task, u, msgtag(L4_PROTO_TASK as i64, 3, 0, 0), timeout_never())
}


/// Revoke rights from a task.
///
/// This method allows to revoke rights from the destination task and from all
/// the tasks that got the rights delegated from that task (i.e., this operation
/// does a recursive rights revocation).
///
/// pre The caller needs to take care that num_fpages is not bigger than
/// `L4_UTCB_GENERIC_DATA_SIZE - 2`.
///
/// **Note:** Calling this function on the object space can cause a root
/// capability of an object to be destructed, which destroys the object itself.
#[inline]
pub unsafe fn l4_task_unmap_batch(task: l4_cap_idx_t, fpages:  *mut l4_fpage_t,
        num_fpages: u32, map_mask: u64) -> l4_msgtag_t {
    l4_task_unmap_batch_u(task, fpages, num_fpages, map_mask, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_unmap_batch_u(task: l4_cap_idx_t,
        fpages: *mut l4_fpage_t, num_fpages: u32, map_mask: u64,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_UNMAP_OP as u64;
    (*v).mr[1] = map_mask as u64;
    ptr::copy_nonoverlapping(fpages as *mut u64, &mut (*v).mr[2], num_fpages as usize);
    l4_ipc_call(task, u, msgtag(L4_PROTO_TASK as i64, 2 + num_fpages, 0, 0),
            timeout_never())
}


/// Release capability and delete object.
///
/// The object `obj` will be deleted from the destination task `task_cap` if the
/// object has sufficient rights. No error will be reported if the rights are
/// insufficient, however, the capability is removed in all cases.
///
/// This operation calls `l4_task_unmap()` with `L4_FP_DELETE_OBJ`.
#[inline]
pub unsafe fn l4_task_delete_obj(task: l4_cap_idx_t, obj: l4_cap_idx_t) ->
        l4_msgtag_t {
    l4_task_delete_obj_u(task, obj, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_delete_obj_u(task: l4_cap_idx_t, obj: l4_cap_idx_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    l4_task_unmap_u(task,
            l4_obj_fpage(obj, 0, L4_CAP_FPAGE_RWSD as u8),
            L4_FP_DELETE_OBJ as u64, u)
}

/// Release capability.
///
/// This operation unmaps the capability from the specified task.
#[inline]
pub unsafe fn l4_task_release_cap(task: l4_cap_idx_t, cap: l4_cap_idx_t)
        -> l4_msgtag_t {
    l4_task_release_cap_u(task, cap, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_release_cap_u(task: l4_cap_idx_t, cap: l4_cap_idx_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    l4_task_unmap_u(task, l4_obj_fpage(cap, 0, L4_CAP_FPAGE_RWSD as u8),
            L4_FP_ALL_SPACES as u64, u)
}

/// Check whether a capability is present in a task (refers to an object).
///
/// A capability is considered present in a task if it refers to an existing
/// kernel object.
#[inline]
pub unsafe fn l4_task_cap_valid(task: l4_cap_idx_t, cap: l4_cap_idx_t)
        -> l4_msgtag_t {
    l4_task_cap_valid_u(task, cap, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_cap_valid_u(task: l4_cap_idx_t, cap: l4_cap_idx_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_CAP_INFO_OP as u64;
    (*v).mr[1] = cap & !1u64;
    l4_ipc_call(task, u, msgtag(L4_PROTO_TASK as i64, 2, 0, 0), timeout_never())
}

/// Test whether two capabilities point to the same object with the same rights.
///
///The returned label of the message tag is 1 on equality, 0 on inequality.
#[inline]
pub unsafe fn l4_task_cap_equal(task: l4_cap_idx_t, cap_a: l4_cap_idx_t,
        cap_b: l4_cap_idx_t) -> l4_msgtag_t {
    l4_task_cap_equal_u(task, cap_a, cap_b, l4_utcb())
}

#[inline]
pub unsafe fn l4_task_cap_equal_u(task: l4_cap_idx_t, cap_a: l4_cap_idx_t,
        cap_b: l4_cap_idx_t, u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_CAP_INFO_OP as u64;
    (*v).mr[1] = cap_a;
    (*v).mr[2] = cap_b;
    l4_ipc_call(task, u, msgtag(L4_PROTO_TASK as i64, 3, 0, 0),
            timeout_never())
}

/// Add kernel-user memory.
///
/// This adds the memory, described by the given flex page, to the specified
/// task.
#[inline]
pub unsafe fn l4_task_add_ku_mem(task: l4_cap_idx_t, ku_mem: l4_fpage_t)
        -> l4_msgtag_t {
    l4_task_add_ku_mem_u(task, ku_mem, l4_utcb())
}

pub unsafe fn l4_task_add_ku_mem_u(task: l4_cap_idx_t, ku_mem: l4_fpage_t,
        u: *mut l4_utcb_t) -> l4_msgtag_t {
    let v = l4_utcb_mr_u(u);
    (*v).mr[0] = L4_TASK_ADD_KU_MEM_OP as u64;
    (*v).mr[1] = ku_mem.raw;
    l4_ipc_call(task, u, msgtag(L4_PROTO_TASK as i64, 2, 0, 0),
            timeout_never())
}


