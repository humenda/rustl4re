#include <asm/generic/smp.h>
#include <asm/generic/smp_ipi.h>
#include <asm/apic.h>

#ifdef CONFIG_X86_64
void default_send_IPI_single(int cpu, int vector);
void default_send_IPI_mask_allbutself_phys(const struct cpumask *mask, int vector);
#endif
