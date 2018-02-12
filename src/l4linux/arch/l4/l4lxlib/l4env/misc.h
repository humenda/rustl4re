/*
 * Inline implementations of include/asm-l4/l4lxapi/misc.h
 * for L4Env.
 */
#ifndef __L4LXLIB__L4ENV__MISC_H__
#define __L4LXLIB__L4ENV__MISC_H__

#include <l4/util/util.h>

L4_INLINE void l4lx_sleep_forever(void)
{
	l4_sleep_forever();
}

#endif /* !__L4LXLIB__L4ENV__MISC_H__ */
