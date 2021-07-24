/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_CURRENT_H
#define __ASM_CURRENT_H

#include <linux/compiler.h>

#ifndef __ASSEMBLY__

#include <linux/build_bug.h>

#ifdef CONFIG_L4
#include <asm/generic/utcb_tcr_id.h>
#include <l4/sys/utcb.h>
#endif /* L4 */

struct task_struct;

/*
 * We don't use read_sysreg() as we want the compiler to cache the value where
 * possible.
 */
static __always_inline struct task_struct *get_current(void)
{
#ifdef CONFIG_L4
	return (struct task_struct *)l4_utcb_tcr()->user[L4X_UTCB_TCR_CURRENT];
#else
	unsigned long sp_el0;

	asm ("mrs %0, sp_el0" : "=r" (sp_el0));

	return (struct task_struct *)sp_el0;
#endif /* L4 */
}

#define current get_current()

#endif /* __ASSEMBLY__ */

#endif /* __ASM_CURRENT_H */

