#ifndef __ASM_L4__GENERIC__SMP_IPI_H__
#define __ASM_L4__GENERIC__SMP_IPI_H__

#include <linux/cpumask.h>

#ifdef CONFIG_X86
#ifdef CONFIG_SMP
void l4x_send_IPI_mask_bitmask(unsigned long, int);
#else
static inline void l4x_send_IPI_mask_bitmask(unsigned long mask, int vector)
{}
#endif
#ifdef CONFIG_X86_64
void l4x_send_IPI_mask(const struct cpumask *cpumask, int vector);
#endif
void __l4x_send_IPI_shortcut(unsigned int shortcut, int vector);
#endif
void l4x_cpu_ipi_setup(unsigned cpu);
void l4x_cpu_ipi_enqueue_vector(unsigned cpu, unsigned vector);
void l4x_cpu_ipi_trigger(unsigned cpu);

#ifdef CONFIG_HOTPLUG_CPU
void l4x_cpu_ipi_stop(unsigned cpu);
#endif

#endif /* __ASM_L4__GENERIC__SMP_IPI_H__ */
