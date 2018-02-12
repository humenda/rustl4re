#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

#include <linux/bitops.h>

/* from asm-i386/ptrace.h */
struct pt_regs;

/* from asm-i386/mach-default/irq-vectors.h */
#ifdef ARCH_arm
#define NR_IRQS 64
#else
#define NR_IRQS 16
#endif

typedef int irqreturn_t;

#define IRQ_NONE       (0)
#define IRQ_HANDLED    (1)
#define IRQ_RETVAL(x)  ((x) != 0)

struct irqaction {
	irqreturn_t (*handler)(int, void *, struct pt_regs *);
	unsigned long flags;
	unsigned long mask;
	const char *name;
	void *dev_id;
	struct irqaction *next;
};

//extern irqreturn_t no_action(int cpl, void *dev_id, struct pt_regs *regs);
extern int request_irq(unsigned int,
		       irqreturn_t (*handler)(int, void *, struct pt_regs *),
		       unsigned long, const char *, void *);
extern void free_irq(unsigned int, void *);

#endif

