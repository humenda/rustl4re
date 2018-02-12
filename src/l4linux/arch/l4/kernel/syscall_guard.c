/*
 * System call guard.
 */

#include <linux/kernel.h>
#include <linux/sched/task_stack.h>

#include <l4/sys/utcb.h>
#include <l4/log/log.h>

#include <asm/generic/dispatch.h>
#include <asm/generic/syscall_guard.h>

typedef int (*sc_check_func_t)(struct task_struct *p);

static int check_invoke(struct task_struct *p)
{
	if (!strcmp(p->comm, "fiasco"))
		return 0;

	return 1;
}

static int check_cs(struct task_struct *p)
{
	if (!strcmp(p->comm, "fiasco"))
		return 0;

	return 1;
}

static int check_debugger(struct task_struct *p)
{
	if (!strcmp(p->comm, "fiasco"))
		return 0;

	return 1;
}

sc_check_func_t sc_check_funcs[] = {
	check_invoke,
	check_cs,
	check_debugger,
};



/*
 * Check if a system call is allow or not.
 *
 * \param p		task structure of the process to check
 * \param utcb		exception utcb state of the process to check
 * \return 0		syscall not allowed
 * \return 1		syscall ok
 */
int l4x_syscall_guard(struct task_struct *p, int sysnr)
{
	if (sysnr >= 0
	    && sysnr < l4x_fiasco_nr_of_syscalls
	    && sc_check_funcs[sysnr]
	    && sc_check_funcs[sysnr](p))
		return 1; /* This syscall is allowed */

	LOG_printf("%s: Syscall%d was forbidden for %s(%d) at %p\n",
	           __func__, sysnr, p->comm, p->pid,
#ifdef CONFIG_ARM
		   (void *)task_pt_regs(p)->ARM_pc
#else
	           (void *)task_pt_regs(p)->ip
#endif
		   );

	return 0; /* Not allowed */
}
