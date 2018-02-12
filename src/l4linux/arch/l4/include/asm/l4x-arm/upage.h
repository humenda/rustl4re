#ifndef __ASM_L4__L4X_ARM__UPAGE_H__
#define __ASM_L4__L4X_ARM__UPAGE_H__

#include <asm/api/config.h>

enum {
	L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_START = 0xfc0,
	L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_END   = 0xfc8,
};

static inline void l4x_kuser_cmpxchg_check_and_fixup(struct pt_regs *regs)
{
	if (unlikely(l4x_is_upage_user_addr(regs->ARM_pc))) {
		/* The used values are sanity checked on bootup */
		unsigned long offs = regs->ARM_pc & (PAGE_SIZE - 1);
		if (   offs >= L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_START
		    && offs <= L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_END)
			regs->ARM_pc = UPAGE_USER_ADDRESS
			               + L4X_UPAGE_KUSER_CMPXCHG_CRITICAL_START;
	}
}

#endif /* ! __ASM_L4__L4X_ARM__UPAGE_H__ */
