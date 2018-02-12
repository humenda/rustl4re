#ifndef __ASM_L4__L4X_ARM__SIGNAL_H__
#define __ASM_L4__L4X_ARM__SIGNAL_H__

#include <linux/linkage.h>
#include <asm/ptrace.h>

int do_signal(struct pt_regs *regs, int syscall);

extern void l4x_show_sigpending_processes(void);

static inline int l4x_do_signal(struct pt_regs *regs, int syscall)
{
	return do_signal(regs, syscall);
}

asmlinkage int
do_work_pending(struct pt_regs *regs, unsigned int thread_flags, int syscall);

#endif /* ! __ASM_L4__L4X_ARM__SIGNAL_H__ */
