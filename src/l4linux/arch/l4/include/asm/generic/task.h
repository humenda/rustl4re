#ifndef __ASM_L4__GENERIC__TASK_H__
#define __ASM_L4__GENERIC__TASK_H__

#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

#include <l4/sys/types.h>

#include <asm/api/config.h>
#include <asm/l4lxapi/task.h>

#ifdef CONFIG_SMP
#include <asm/generic/smp.h>
#define l4x_idle_task(cpu) l4x_cpu_idle_get(cpu)
#else
#define l4x_idle_task(cpu) (&init_task)
#endif

void l4x_exit_thread(void);
void l4x_evict_tasks(struct task_struct *exclude);

#ifdef CONFIG_L4_VCPU
static inline void l4x_init_thread_struct(struct task_struct *p) {}
#else

DECLARE_PER_CPU(struct thread_info *, l4x_current_ti);
void l4x_init_thread_struct(struct task_struct *p);

static inline void
l4x_delete_process_thread(struct task_struct *tsk)
{
	int i;

	for (i = 0; i < NR_CPUS; i++) {
		l4_cap_idx_t thread_id = tsk->thread.l4x.user_thread_ids[i];

		if (l4_is_invalid_cap(thread_id))
			continue;

		if (l4lx_task_delete_thread(thread_id))
			do_exit(9);

		l4lx_task_number_free(thread_id);
		tsk->thread.l4x.user_thread_ids[i] = L4_INVALID_CAP;
	}

	tsk->thread.l4x.started = 0;
	tsk->thread.l4x.threads_up = 0;
	tsk->thread.l4x.user_thread_id = L4_INVALID_CAP;
}
#endif /* VCPU */

#endif /* ! __ASM_L4__GENERIC__TASK_H__ */
