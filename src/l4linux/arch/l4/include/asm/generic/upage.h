#ifndef __ASM_L4__GENERIC__UPAGE_H__
#define __ASM_L4__GENERIC__UPAGE_H__

#include <linux/compiler.h>

#include <asm/api/config.h>

int l4x_peek_upage(unsigned long addr, unsigned long __user *datap, int *ret);

extern unsigned long upage_addr;

static inline int l4x_is_upage_user_addr(unsigned long addr)
{
	return (addr & PAGE_MASK) == UPAGE_USER_ADDRESS;
}

#endif /* ! __ASM_L4__GENERIC__UPAGE_H__ */
