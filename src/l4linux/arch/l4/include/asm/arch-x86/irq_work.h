/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_IRQ_WORK_H
#define _ASM_IRQ_WORK_H

#ifndef CONFIG_L4
#include <asm/cpufeature.h>
#endif

#ifdef CONFIG_X86_LOCAL_APIC
static inline bool arch_irq_work_has_interrupt(void)
{
#ifdef CONFIG_L4
	return true;
#else
	return boot_cpu_has(X86_FEATURE_APIC);
#endif
}
extern void arch_irq_work_raise(void);
#else
static inline bool arch_irq_work_has_interrupt(void)
{
	return false;
}
#endif

#endif /* _ASM_IRQ_WORK_H */
