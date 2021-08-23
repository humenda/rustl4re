//! L4(Re) Task API

use libc::l4_umword_t;

use l4_sys::{self, l4_cap_consts_t::L4_CAP_SHIFT,
             l4_addr_t, l4_fpage_t, l4_cap_idx_t};
use crate::cap::{Cap, CapIdx, Interface};
use crate::error::{Result};
use crate::ipc::MsgTag;
use crate::types::{UMword};
use crate::utcb::Utcb;

// ToDo
pub static THIS_TASK: Cap<Task> = Cap {
    interface: Task {
        cap: (1 as l4_umword_t) << L4_CAP_SHIFT as l4_umword_t,
    }
};


/// Task kernel object
/// The `Task` represents a combination of the address spaces provided
/// by the L4Re micro kernel. A task consists of at least a memory address space
/// and an object address space. On IA32 there is also an IO-port address space
/// associated with an L4::Task.
///
/// Task objects are created using the Factory interface.
///
pub struct Task {
    // ToDo: a non-public, private capability handle; this is unsafe and should be rethought
    cap: l4_cap_idx_t,
}
// ToDo: inherits from:
//  public Kobject_t<Task, Kobject, L4_PROTO_TASK,
//                   Type_info::Demand_t<2> >

impl Interface for Task {
    #[inline]
    unsafe fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl Task {
    /// Map resources available in the source task to a destination task.
    /// 
    /// The sendbase describes an offset within  the receive window of the reciving task and the
    /// flex page contains the capability or memory being transfered.
    ///
    /// This method allows for asynchronous rights delegation from one task to another. It can be
    /// used to share memory as well as to delegate access to objects.
    /// The destination task is the task referenced by the capability invoking map and the receive
    /// window is the whole address space of said task.
    #[inline]
    pub unsafe fn map_u(&self, dst_task: Cap<Task>, snd_fpage: l4_fpage_t,
                 snd_base: l4_addr_t, u: &mut Utcb) -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_map_u(dst_task.raw(), self.cap,
                snd_fpage, snd_base, u.raw)).result()
    }

    /// See `map_u`
    #[inline]
    pub unsafe fn map(&self, src_task: Cap<Task>, snd_fpage: l4_fpage_t,
               snd_base: l4_addr_t) -> Result<MsgTag> {
        self.map_u(src_task, snd_fpage, snd_base, &mut Utcb::current())
    }

    /// See `unmap`
    #[inline]
    pub unsafe fn unmap_u(&self, fpage: l4_fpage_t, map_mask: UMword,
                   u: &mut Utcb) -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_unmap_u(self.cap, fpage, map_mask as l4_umword_t,
                u.raw)).result()
    }

    /// Revoke rights from the task.
    ///
    /// This method allows to revoke rights from the destination task and from all the tasks that
    /// got the rights delegated from that task (i.e., this operation does a recursive rights
    /// revocation). The flex page argument has to describe a reference a resource of *this* task.
    ///
    /// If the capability possesses delete rights or if it is the last capability pointing to
    /// the object, calling this function might
    ///       destroy the object itself.
    #[inline]
    pub unsafe fn unmap(&self, fpage: l4_fpage_t, map_mask: UMword) -> Result<MsgTag> {
        self.unmap_u(fpage, map_mask, &mut Utcb::current())
    }

    /// Revoke rights from a task.
    ///
    /// This method allows to revoke rights from the destination task and from all the tasks that
    /// got the rights delegated from that task (i.e., this operation does a recursive rights
    /// revocation). The given flex pages need to be present in this task.
    ///
    /// The caller needs to take care that `num_fpages` is not bigger than
    /// `L4_UTCB_GENERIC_DATA_SIZE - 2`.
    ///
    /// If the capability possesses delete rights or if it is the last capability pointing to the
    /// object, calling this function might destroy the object itself.
    #[inline]
    pub unsafe fn unmap_batch(&self, fpages: &mut l4_fpage_t, num_fpages: usize,
            map_mask: UMword, utcb: &Utcb) -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_unmap_batch_u(self.cap, fpages,
                num_fpages as u32, map_mask as u64, utcb.raw)).result()
    }

    /// Release capability and delete object.
    ///
    /// The object will be deleted if the `obj` has sufficient rights. No error will be reported if
    /// the rights are insufficient, however, the capability is removed in all cases.
    #[inline]
    pub unsafe fn delete_obj<T: Interface>(&self, obj: Cap<T>, utcb: &Utcb)
            -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_delete_obj_u(self.cap, obj.raw(), utcb.raw))
                .result()
    }

    /// Release capability.
    ///
    /// This operation unmaps the capability from `this` task.
    #[inline]
    pub unsafe fn release_cap<T: Interface>(&self, cap: Cap<T>, u: &Utcb)
            -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_release_cap_u(self.cap, cap.raw(), u.raw))
                .result()
    }

    /// Check whether a capability is present (refers to an object).
    ///
    /// A capability is considered present when it refers to an existing kernel object.
    pub fn cap_valid<T: Interface>(&self, cap: &Cap<T>, utcb: &Utcb)
            -> Result<bool>  {
        let tag: Result<MsgTag> = unsafe {
            MsgTag::from(l4_sys::l4_task_cap_valid_u(
                    self.cap, cap.raw(), utcb.raw)).result()
        };
        let tag: MsgTag = tag?;
        Ok(tag.label() > 0)
    }

    /// Check capability equality across task boundaries
    ///
    /// Test whether two capabilities point to the same object with the same rights. The UTCB is
    /// that one of the calling thread.
    pub fn cap_equal<T: Interface>(&self, cap_a: &Cap<T>, cap_b: &Cap<T>,
                        utcb: &Utcb) -> Result<bool> {
        let tag: Result<MsgTag> = unsafe {
            MsgTag::from(l4_sys::l4_task_cap_equal_u(
                        self.cap, cap_a.raw(), cap_b.raw(), utcb.raw)).result()
        };
        let tag: MsgTag = tag?;
        Ok(tag.label() == 1)
    }

    /// Add kernel-user memory.
    ///
    /// This adds user-kernel memory (to be used for instance as UTCB) by specifying it in the
    /// given flex page.
    pub unsafe fn add_ku_mem(&self, fpage: l4_fpage_t, utcb: &Utcb) -> Result<MsgTag> {
        MsgTag::from(l4_sys::l4_task_add_ku_mem_u(self.cap, fpage,
                utcb.raw)).result()
    }

//    /// Create a L4 task
//    ///
//    /// This creates a L4 task, mapping its own capability and the parent capability into the
//    /// object space for later use.
//    pub unsafe fn create_from<T: Interface>(mut task_cap: Cap<Untyped>,
//            utcb_area: l4_fpage_t) -> Result<Task> {
//        let tag = MsgTag::from(l4_sys::l4_factory_create_task(
//                l4re_env()?.factory, &mut task_cap.raw(), utcb_area)).result()?;
//        panic!("ToDo: the first two need to be reimplemented, the last one is up to the caller");
//        //map_obj_to_other(task_cap, pager_gate, pager_id, "pager"); /* Map pager cap */
//        //map_obj_to(task_cap, main_id, L4_FPAGE_RO, "main"); /* Make us visible */
//        //map_obj_to(task_cap, l4re_env()->log, L4_FPAGE_RO, "log"); /* Make print work */
//    }
}

/*
pub fn create_task() -> Result<Cap<Task>> {
    let newtask = Cap::alloc();
    let _ = MsgTag::from(l4_sys::l4_factory_create_task(l4re_env()?.factory,
            &mut newtask.raw(),
            ToDo_created_utcb)).result()?;
    // map our region manager to child, use C-alike function to avoid creation of task object for
    // current task
    let _ = l4_sys::l4_task_map(newtask.raw(), THIS_TASK,
            l4_obj_fpage((*l4re_env()).rm, 0, L4_FPAGE_RO),
            l4_map_obj_control((*l4re_env()).rm, l4_sys::L4_MAP_ITEM_MAP)).result()?;
}
*/

