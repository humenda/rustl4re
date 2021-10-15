#ifndef L4RE_HIKEY960_SCHEDULER_H
#define L4RE_HIKEY960_SCHEDULER_H

#include <l4/sys/scheduler.h>
#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN l4_msgtag_t
l4_scheduler_info_w(l4_cap_idx_t scheduler, l4_umword_t *cpu_max, l4_sched_cpu_set_t *cpus);

EXTERN l4_msgtag_t
l4_scheduler_run_thread_w(l4_cap_idx_t scheduler, l4_cap_idx_t thread, l4_sched_param_t const *sp);

EXTERN l4_sched_cpu_set_t
l4_sched_cpu_set_w(l4_umword_t offset, unsigned char granularity, l4_umword_t map);

#endif //L4RE_HIKEY960_SCHEDULER_H
