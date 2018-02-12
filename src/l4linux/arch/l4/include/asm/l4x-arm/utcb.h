#ifndef __ASM_L4__L4X_ARM__UTCB_H__
#define __ASM_L4__L4X_ARM__UTCB_H__

#include <l4/sys/utcb.h>
#include <asm/bug.h>

static inline void l4x_utcb_set(l4_cap_idx_t t, l4_utcb_t *u)
{
	BUG();
}

static inline l4_utcb_t *l4x_utcb_get(l4_cap_idx_t t)
{
	BUG();
	return NULL;
}

#endif /* ! __ASM_L4__L4X_ARM__UTCB_H__ */
