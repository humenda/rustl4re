#ifndef __ASM_L4__L4X_I386__UTCB_H__
#define __ASM_L4__L4X_I386__UTCB_H__

#include <asm/l4lxapi/generic/thread_gen.h>

unsigned l4x_x86_utcb_get_orig_segment(void);


enum { L4X_UTCB_POINTERS = L4LX_THREAD_NO_THREADS + 3 /* l4env threads */ };

extern l4_utcb_t *l4x_utcb_pointer[L4X_UTCB_POINTERS];

static inline void l4x_utcb_set(l4_cap_idx_t t, l4_utcb_t *u)
{
	BUG();
	BUG_ON((t >> L4_CAP_SHIFT) >= L4X_UTCB_POINTERS);
	l4x_utcb_pointer[t >> L4_CAP_SHIFT] = u;
}

static inline l4_utcb_t *l4x_utcb_get(l4_cap_idx_t t)
{
	BUG();
	return l4x_utcb_pointer[t >> L4_CAP_SHIFT];
}

#endif /* ! __ASM_L4__L4X_I386__UTCB_H__ */
