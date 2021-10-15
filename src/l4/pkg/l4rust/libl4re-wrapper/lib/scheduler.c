#include <l4/l4rust/scheduler.h>

EXTERN l4_msgtag_t
l4_scheduler_info_w(l4_cap_idx_t scheduler, l4_umword_t *cpu_max, l4_sched_cpu_set_t *cpus) {
    return l4_scheduler_info(scheduler, cpu_max, cpus);
}

EXTERN l4_msgtag_t
l4_scheduler_run_thread_w(l4_cap_idx_t scheduler, l4_cap_idx_t thread, l4_sched_param_t const *sp) {
    return l4_scheduler_run_thread(scheduler, thread, sp);

}

EXTERN l4_sched_cpu_set_t
l4_sched_cpu_set_w(l4_umword_t offset, unsigned char granularity, l4_umword_t map) {
    return l4_sched_cpu_set(offset, granularity, map);
}