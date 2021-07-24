#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/spinlock.h>

#include <asm/api/macros.h>
#include <asm/l4x/signal.h>
#include <l4/sys/kdebug.h>

#if defined(CONFIG_X86)
int l4x_deliver_signal(int exception_nr)
{
	force_sig_fault(SIGSEGV, SEGV_MAPERR,
	                (void __user *)task_pt_regs(current)->ip);

	if (signal_pending(current)) {
		arch_do_signal_or_restart(task_pt_regs(current), true);
		return 1;
	}

	return 0;
}
#elif defined(CONFIG_ARM)
int l4x_deliver_signal(int exception_nr)
{
	force_sig_fault(SIGSEGV, SEGV_MAPERR,
	                (void __user *)task_pt_regs(current)->ARM_pc);

	if (signal_pending(current)) {
		do_signal(task_pt_regs(current), 0);
		return 1;
	}

	return 0;
}
#elif defined(CONFIG_ARM64)
int l4x_deliver_signal(int exception_nr)
{
	force_sig_fault(SIGSEGV, SEGV_MAPERR,
	                (void __user *)task_pt_regs(current)->pc);

	if (signal_pending(current)) {
		do_signal(task_pt_regs(current));
		return 1;
	}

	return 0;
}
#else
#error Unknown arch
#endif
