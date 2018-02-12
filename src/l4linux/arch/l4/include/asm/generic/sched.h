#ifndef __ASM_L4__GENERIC__SCHED_H__
#define __ASM_L4__GENERIC__SCHED_H__

#include <linux/thread_info.h>

#include <asm/generic/kthreads.h>
#include <asm/generic/dispatch.h>

#include <l4/sys/kdebug.h>
#include <l4/sys/ipc.h>

#ifndef CONFIG_L4_VCPU
static inline int l4x_in_kernel(void)
{
	unsigned cpu = smp_processor_id();
	if (!per_cpu(l4x_current_proc_run, cpu))
		return true;

#ifdef CONFIG_THREAD_INFO_IN_TASK
	return !((struct task_struct *)(per_cpu(l4x_current_proc_run, cpu)))->mm;
#else
	return !per_cpu(l4x_current_proc_run, cpu)->task->mm;
#endif
}

/*
 * IRQ threads use this routine to check if they
 * need to wake up a sleeping kernel
 */
static inline void l4x_wakeup_idle_if_needed(void)
{
	int cpu;

	/* when in an irq service routine, we must make sure the
	 * wakeup request will really wake up the process.  so if the
	 * kernel server is idling, wake it up. */

	for_each_online_cpu(cpu) {
		struct thread_info *t;
		barrier();
		t = per_cpu(l4x_current_proc_run, cpu);
		/* Check if server is waiting and we have work to do */
		if (t
#ifdef CONFIG_X86
		    && (_TIF_ALLWORK_MASK & t->flags)
#elif defined(CONFIG_ARM)
		    && (_TIF_WORK_MASK & t->flags)
#else
#error Unknown arch
#endif
		    ) {
			if (per_cpu(l4x_idle_running, cpu)) {
				/*
				 * No user process is currently running,
				 * i.e.  idle is only waiting for interrupts
				 * to go on.
				 */
				l4x_wakeup_idler(cpu);
			} else {
				/*
				 * A user process is currently running, go
				 * interrupt it so that it comes in and
				 * triggers any possible interrupt work to
				 * do.
				 */
#ifdef CONFIG_THREAD_INFO_IN_TASK
				l4x_suspend_user((struct task_struct *)t, cpu);
#else
				l4x_suspend_user(t->task, cpu);
#endif
			}
		}
	}
}
#endif /* vcpu */

#endif /* ! __ASM_L4__GENERIC__SCHED_H__ */
