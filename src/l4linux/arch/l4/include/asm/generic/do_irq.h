#ifndef __ASM_L4__GENERIC__DO_IRQ_H__
#define __ASM_L4__GENERIC__DO_IRQ_H__

#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include <linux/sched/task_stack.h>

#include <asm/irq.h>
#ifdef CONFIG_X86
#include <asm/idtentry.h>
#endif /* X86 */

#include <asm/generic/sched.h>
#include <asm/generic/task.h>
#include <asm/l4lxapi/irq.h>
#include <asm/l4x/exception.h>

#ifndef CONFIG_L4_VCPU

#ifdef CONFIG_X86
void l4x_x86_smp_process_ipi(int vector, struct pt_regs *regs);
#endif /* X86 */

static inline
struct task_struct *l4x_do_I_setup_task(void)
{
#ifdef CONFIG_THREAD_INFO_IN_TASK
	struct thread_info *ti = per_cpu(l4x_current_ti, smp_processor_id());
	l4_utcb_tcr()->user[L4X_UTCB_TCR_CURRENT] = (unsigned long)ti;
	return (struct task_struct *)ti;
#else
	struct thread_info *ti;
	unsigned long sp = current_stack_pointer;
	ti = (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
	return ti->task = per_cpu(l4x_current_ti, smp_processor_id())->task;
#endif
}

static inline void __l4x_do_IRQ_generic(int irq, bool is_ipi)
{
	unsigned long flags, old_cpu_state;
	struct pt_regs *r;
	struct task_struct *t;

	local_irq_save(flags);
	t = l4x_do_I_setup_task();
	r = task_pt_regs(t);
	old_cpu_state = l4x_get_cpu_mode(r);
	l4x_set_cpu_mode(r, l4x_in_kernel() ? L4X_MODE_KERNEL : L4X_MODE_USER);
#ifdef CONFIG_X86
#ifdef CONFIG_SMP
	if (is_ipi) // is_ipi is only for x86, others use generic IRQs
		l4x_x86_smp_process_ipi(irq, r);
	else
#endif /* SMP */
		common_interrupt(r, irq);
#elif defined(CONFIG_ARM64)
	__handle_domain_irq(NULL, irq, false, r);
#else
	handle_IRQ(irq, r);
#endif
	l4x_set_cpu_mode(r, old_cpu_state);
	local_irq_restore(flags);

	l4x_wakeup_idle_if_needed();
}

static inline void l4x_do_IRQ(int irq)
{
	__l4x_do_IRQ_generic(irq, 0);
}

#ifdef CONFIG_SMP
static inline void l4x_do_IPI(int irq)
{
	__l4x_do_IRQ_generic(irq, 1);
}
#endif /* SMP */
#endif /* !vcpu */

#endif /* ! __ASM_L4__GENERIC__DO_IRQ_H__ */
