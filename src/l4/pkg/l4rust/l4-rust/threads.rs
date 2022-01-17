use crate::sys::{l4_cap_idx_t, pthread_l4_cap, l4_is_invalid_cap, l4_sched_param_t,
                 l4_sched_param, l4_cpu_time_t, l4_umword_t, l4_sched_cpu_set,
                 l4_scheduler_run_thread, l4re_global_env, l4_msgtag_t, l4_error,
                 l4_scheduler_info, l4_sched_cpu_set_t};
use std::os::unix::thread::JoinHandleExt;
use std::thread::JoinHandle;
use libc::{pthread_t, c_char, c_uint};

#[derive(Debug, Clone, Copy)]
pub enum CpuSetError{
    InvalidCap(),
    RunError(l4_msgtag_t)
}

pub fn sched_cpu_set<T>(handle: &mut JoinHandle<T>, core: l4_umword_t, prio: c_uint, quantum: l4_cpu_time_t,
                        granularity: c_char, map: l4_umword_t) -> Result<(), CpuSetError>{
    let id = handle.as_pthread_t() as pthread_t;
    let cap = unsafe {pthread_l4_cap(id)};
    if l4_is_invalid_cap(cap) {
        return Err(CpuSetError::InvalidCap())
    }

    let mut sp: l4_sched_param_t = l4_sched_param(prio, quantum);
    sp.affinity = l4_sched_cpu_set(core, granularity, map);

    let scheduler: l4_cap_idx_t = unsafe { (*l4re_global_env).scheduler };

    let msg: l4_msgtag_t = l4_scheduler_run_thread(scheduler, cap, &sp);
    if l4_error(msg) > 0{
        return Err(CpuSetError::RunError(msg));
    }
    Ok(())
}

pub fn num_cores(core: l4_umword_t, granularity: c_char, map: l4_umword_t) -> Result<(l4_umword_t, l4_sched_cpu_set_t), l4_msgtag_t> {
    let scheduler: l4_cap_idx_t = unsafe { (*l4re_global_env).scheduler };
    let mut sp: l4_sched_cpu_set_t = l4_sched_cpu_set(core, granularity, map);
    let mut num_cores: l4_umword_t = 0;
    let msg = l4_scheduler_info(scheduler, &mut num_cores, &mut sp);
    if l4_error(msg) > 0{
        return Err(msg)
    }
    Ok((num_cores, sp))
}