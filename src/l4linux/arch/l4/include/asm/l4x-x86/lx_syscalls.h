#ifndef __ASM_L4__L4X__I386__LX_SYSCALLS_H__
#define __ASM_L4__L4X__I386__LX_SYSCALLS_H__

#include <asm/syscall.h>

typedef asmlinkage long (*syscall_t)(long a0,...);

static inline int is_lx_syscall(unsigned nr)
{
	return (nr & __SYSCALL_MASK) < NR_syscalls;
}

#endif /* ! __ASM_L4__L4X__I386__LX_SYSCALLS_H__ */
