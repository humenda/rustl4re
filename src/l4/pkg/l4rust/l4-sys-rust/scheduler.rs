use crate::{l4_sched_param_w, l4_cpu_time_t, l4_umword_t, l4_sched_param_t,
            l4_sched_cpu_set_w, l4_scheduler_run_thread_w, l4_sched_cpu_set_t,
            l4_scheduler_info_w, l4_msgtag_t, l4_cap_idx_t};
use libc::{c_char, c_uint};

#[must_use]
#[inline]
pub fn l4_sched_param(prio: c_uint, quantum: l4_cpu_time_t) -> l4_sched_param_t {
    //Not unsafe because this function is marked as L4_NOTHROW
    unsafe {
        l4_sched_param_w(prio, quantum)
    }
}

#[must_use]
#[inline]
pub fn l4_sched_cpu_set(offset: l4_umword_t, granularity: c_char, map: l4_umword_t) -> l4_sched_cpu_set_t {
    //Not unsafe because this function is marked as L4_NOTHROW
    unsafe {
        l4_sched_cpu_set_w(offset, granularity, map)
    }
}

#[must_use]
#[inline]
pub fn l4_scheduler_run_thread(scheduler: l4_cap_idx_t, thread: l4_cap_idx_t, sp: *const l4_sched_param_t) -> l4_msgtag_t {
    //Not unsafe because this function is marked as L4_NOTHROW
    unsafe {
        l4_scheduler_run_thread_w(scheduler, thread, sp)
    }
}

#[must_use]
#[inline]
pub fn l4_scheduler_info(scheduler: l4_cap_idx_t, cpu_max: &mut l4_umword_t,
        cpus: &mut l4_sched_cpu_set_t) -> l4_msgtag_t {
    //Not unsafe because this function is marked as L4_NOTHROW
    unsafe {
        l4_scheduler_info_w(scheduler, cpu_max, cpus)
    }
}