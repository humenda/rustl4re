/*
 * Inline implementations of the API defined in asm/l4lxapi/thread.h
 * for L4Env.
 */
#ifndef __L4LXLIB__L4ENV__THREAD_H__
#define __L4LXLIB__L4ENV__THREAD_H__

#include <linux/build_bug.h>
#include <asm/generic/stack_id.h>

L4_INLINE int l4lx_thread_equal(l4_cap_idx_t t1, l4_cap_idx_t t2)
{
	return l4_capability_equal(t1, t2);
}

L4_INLINE int l4lx_thread_is_valid(l4lx_thread_t t)
{
	return (int)(unsigned long)t;
}

static inline l4_cap_idx_t l4lx_thread_get_cap(l4lx_thread_t t)
{
	return l4x_cap_current_utcb(t);
}

#endif /* !__L4LXLIB__L4ENV__THREAD_H__ */
