#ifndef __INCLUDE__ASM__GENERIC__FUTEX_HELPER_H__
#define __INCLUDE__ASM__GENERIC__FUTEX_HELPER_H__

#include <asm/generic/memory.h>

static inline
u32 __user *l4x_futex_translate_uaddr_nocheck(u32 __user *uaddr,
                                              unsigned long *flags)
{
	unsigned long page, offset;
	page = parse_ptabs_write((unsigned long)uaddr, &offset, flags);
	if (page == -EFAULT)
		return ERR_PTR(-EFAULT);
	return (u32 __user *)(page + offset);
}

static inline
void l4x_futex_translate_uaddr_end(unsigned long flags)
{
	local_irq_restore(flags);
}

#endif /* ! __INCLUDE__ASM__GENERIC__FUTEX_HELPER_H__ */
