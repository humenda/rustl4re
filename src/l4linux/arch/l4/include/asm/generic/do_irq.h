#ifndef __ASM_L4__GENERIC__DO_IRQ_H__
#define __ASM_L4__GENERIC__DO_IRQ_H__

#include <linux/spinlock.h>
#include <linux/thread_info.h>

#include <asm/irq.h>

#include <asm/generic/sched.h>
#include <asm/generic/task.h>
#include <asm/l4lxapi/irq.h>
#include <asm/l4x/exception.h>

#ifndef CONFIG_L4_VCPU

static inline
struct task_struct *l4x_do_I_setup_task(unsigned long sp, int cpu)
{
#ifdef CONFIG_THREAD_INFO_IN_TASK
	return (struct task_struct *)per_cpu(l4x_current_ti, cpu);
#else
	struct thread_info *ti;
	ti = (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
	return ti->task = per_cpu(l4x_current_ti, cpu)->task;
#endif
}

static inline void l4x_do_IRQ(int irq)
{
	unsigned long flags, old_cpu_state;
	struct pt_regs *r;
	int cpu = smp_processor_id();
	struct task_struct *t;

	local_irq_save(flags);
	t = l4x_do_I_setup_task(current_stack_pointer, cpu);
	r = task_pt_regs(t);
	old_cpu_state = l4x_get_cpu_mode(r);
	l4x_set_cpu_mode(r, l4x_in_kernel() ? L4X_MODE_KERNEL : L4X_MODE_USER);
#ifdef CONFIG_X86
	do_IRQ(irq, r);
#else
	handle_IRQ(irq, r);
#endif
	l4x_set_cpu_mode(r, old_cpu_state);
	local_irq_restore(flags);

	l4x_wakeup_idle_if_needed();
}

#ifdef CONFIG_SMP
#include <asm/generic/smp.h>

static inline void l4x_do_IPI(int vector)
{
	unsigned long flags, old_cpu_state;
	struct pt_regs *r;
	int cpu = smp_processor_id();
	struct task_struct *t;

	local_irq_save(flags);
	t = l4x_do_I_setup_task(current_stack_pointer, cpu);
	r = task_pt_regs(t);
	old_cpu_state = l4x_get_cpu_mode(r);
	l4x_set_cpu_mode(r, l4x_in_kernel() ? L4X_MODE_KERNEL : L4X_MODE_USER);
	l4x_smp_process_IPI(vector, r);
	l4x_set_cpu_mode(r, old_cpu_state);
	local_irq_restore(flags);

	l4x_wakeup_idle_if_needed();
}
#endif
#endif /* !vcpu */

#endif /* ! __ASM_L4__GENERIC__DO_IRQ_H__ */
