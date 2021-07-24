/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_CACHEFLUSH_H
#define _ASM_X86_CACHEFLUSH_H

#include <linux/mm.h>

/* Caches aren't brain-dead on the intel. */
#ifdef CONFIG_L4
#include <asm/cacheflush-generic.h>
#else /* L4 */
#include <asm-generic/cacheflush.h>
#endif /* L4 */
#include <asm/special_insns.h>

void clflush_cache_range(void *addr, unsigned int size);

#endif /* _ASM_X86_CACHEFLUSH_H */
