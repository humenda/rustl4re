#ifndef __ASM_L4__GENERIC__DISPATCH_H__
#define __ASM_L4__GENERIC__DISPATCH_H__

#include <linux/linkage.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/thread_info.h>

#define TBUF_CAP(x) ((((x) >> L4_CAP_SHIFT) == 0xfffff) ? 0xffff : ((x) >> L4_CAP_SHIFT))
#ifdef CONFIG_L4_VCPU
#define TBUF_TID(tid) 0
#else
#define TBUF_TID(tid) TBUF_CAP(tid)
#endif

#define TBUF_DO_IT(x) do { x; } while (0)

#ifndef CONFIG_L4_VCPU
void l4x_idle(void);
void l4x_suspend_user(struct task_struct *p, int cpu);
void l4x_wakeup_idler(int cpu);
#else
int l4x_vcpu_handle_kernel_exc(l4_vcpu_regs_t *vr);
#endif

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
void l4x_print_vm_area_maps(struct task_struct *p, unsigned long highlight);
#endif

#ifndef CONFIG_L4_VCPU
DECLARE_PER_CPU(struct thread_info *, l4x_current_proc_run);
DECLARE_PER_CPU(int, l4x_idle_running);
#endif

extern unsigned l4x_fiasco_nr_of_syscalls;

extern l4_cap_idx_t l4x_user_gate[NR_CPUS];

#endif /* ! __ASM_L4__GENERIC__DISPATCH_H__ */
