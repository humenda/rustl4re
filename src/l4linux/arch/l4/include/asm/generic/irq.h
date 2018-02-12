#ifndef __ASM_L4__GENERIC__IRQ_H__
#define __ASM_L4__GENERIC__IRQ_H__

#include <asm/generic/kthreads.h>

#ifdef CONFIG_L4_VCPU
#define L4X_VCPU_IRQ_IPI (NR_IRQS)
#else
#define L4_IRQ_DISABLED 0
#define L4_IRQ_ENABLED  1
#endif

int l4x_register_irq(l4_cap_idx_t irqcap);
int l4x_alloc_percpu_irq(l4_cap_idx_t __percpu **percpucaps);
void l4x_free_percpu_irq(int irq);
int l4x_register_percpu_irqcap(int irq, unsigned cpu, l4_cap_idx_t cap);
void l4x_unregister_irq(int irqnum);
l4_cap_idx_t l4x_have_irqcap(int irqnum, unsigned cpu);

struct irq_data;
struct irq_chip;

struct l4x_irq_desc_private {
	unsigned is_percpu;
	union {
		l4_cap_idx_t irq_cap;
		l4_cap_idx_t __percpu *irq_caps;
	} c;
	l4_cap_idx_t icu;
#ifndef CONFIG_L4_VCPU
	l4lx_thread_t irq_thread;
#endif
	unsigned enabled;
	unsigned cpu;
	unsigned char trigger;
};

struct l4x_irq_desc_private *
l4x_alloc_irq_desc_data(int lx_irq, l4_cap_idx_t icu, unsigned icu_irq);
void l4x_free_irq_dest_data(struct l4x_irq_desc_private *p);
l4_cap_idx_t l4x_irq_init(l4_cap_idx_t icu, unsigned icu_irq, unsigned trigger,
                          char *tag);
int l4x_init_dyn_irq(int lx_irq, l4_cap_idx_t irqcap, l4_cap_idx_t icucap,
                     unsigned icu_irq, struct irq_chip *chip);
void l4x_irq_deinit(struct irq_data *data);
int l4x_irq_release(l4_cap_idx_t irqcap);

void l4x_init_IRQ(void);

extern struct irq_chip l4x_irq_icu_chip;
extern struct irq_chip l4x_irq_plain_chip;
#ifdef CONFIG_PCI_MSI
extern struct irq_chip l4x_irq_msi_chip;
#endif

#endif /* ! __ASM_L4__GENERIC__IRQ_H__ */
