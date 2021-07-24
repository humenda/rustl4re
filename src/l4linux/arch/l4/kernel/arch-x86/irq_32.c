// SPDX-License-Identifier: GPL-2.0
/*
 *	Copyright (C) 1992, 1998 Linus Torvalds, Ingo Molnar
 *
 * This file contains the lowest level x86-specific interrupt
 * entry, irq-stacks and irq statistics code. All the remaining
 * irq logic is done by the generic kernel/irq/ code and
 * by the x86-specific irq controller code. (e.g. i8259.c and
 * io_apic.c.)
 */

#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/percpu.h>
#include <linux/mm.h>

#include <asm/apic.h>
#include <asm/nospec-branch.h>
#include <asm/softirq_stack.h>

#ifdef CONFIG_L4
#include <asm/generic/stack_id.h>
#include <asm/generic/task.h>
#endif /* L4 */

#ifdef CONFIG_DEBUG_STACKOVERFLOW

int sysctl_panic_on_stackoverflow __read_mostly;

/* Debugging check for stack overflow: is there less than 1KB free? */
static int check_stack_overflow(void)
{
	long sp;

	__asm__ __volatile__("andl %%esp,%0" :
			     "=r" (sp) : "0" (THREAD_SIZE - 1));

	return sp < (sizeof(struct thread_info) + STACK_WARN);
}

static void print_stack_overflow(void)
{
	printk(KERN_WARNING "low stack detected by irq handler\n");
	dump_stack();
	if (sysctl_panic_on_stackoverflow)
		panic("low stack detected by irq handler - check messages\n");
}

#else
static inline int check_stack_overflow(void) { return 0; }
static inline void print_stack_overflow(void) { }
#endif

DEFINE_PER_CPU(struct irq_stack *, hardirq_stack_ptr);
DEFINE_PER_CPU(struct irq_stack *, softirq_stack_ptr);

static void call_on_stack(void *func, void *stack)
{
	asm volatile("xchgl	%%ebx,%%esp	\n"
		     CALL_NOSPEC
		     "movl	%%ebx,%%esp	\n"
		     : "=b" (stack)
		     : "0" (stack),
		       [thunk_target] "D"(func)
		     : "memory", "cc", "edx", "ecx", "eax");
}

static inline void *current_stack(void)
{
	return (void *)(current_stack_pointer & ~(THREAD_SIZE - 1));
}

static inline int execute_on_irq_stack(int overflow, struct irq_desc *desc)
{
	struct irq_stack *curstk, *irqstk;
	u32 *isp, *prev_esp, arg1;

	curstk = (struct irq_stack *) current_stack();
	irqstk = __this_cpu_read(hardirq_stack_ptr);

	/*
	 * this is where we switch to the IRQ stack. However, if we are
	 * already using the IRQ stack (because we interrupted a hardirq
	 * handler) we can't do that and just have to keep using the
	 * current stack (which is the irq stack already after all)
	 */
	if (unlikely(curstk == irqstk))
		return 0;

	isp = (u32 *) ((char *)irqstk + sizeof(*irqstk));

	/* Save the next esp at the bottom of the stack */
	prev_esp = (u32 *)irqstk;
	*prev_esp = current_stack_pointer;

	if (unlikely(overflow))
		call_on_stack(print_stack_overflow, isp);

	asm volatile("xchgl	%%ebx,%%esp	\n"
		     CALL_NOSPEC
		     "movl	%%ebx,%%esp	\n"
		     : "=a" (arg1), "=b" (isp)
		     :  "0" (desc),   "1" (isp),
			[thunk_target] "D" (desc->handle_irq)
		     : "memory", "cc", "ecx");
	return 1;
}

/*
 * Allocate per-cpu stacks for hardirq and softirq processing
 */
int irq_init_percpu_irqstack(unsigned int cpu)
{
	int node = cpu_to_node(cpu);
	struct page *ph, *ps;

	if (per_cpu(hardirq_stack_ptr, cpu))
		return 0;

	ph = alloc_pages_node(node, THREADINFO_GFP, THREAD_SIZE_ORDER);
	if (!ph)
		return -ENOMEM;
	ps = alloc_pages_node(node, THREADINFO_GFP, THREAD_SIZE_ORDER);
	if (!ps) {
		__free_pages(ph, THREAD_SIZE_ORDER);
		return -ENOMEM;
	}

	per_cpu(hardirq_stack_ptr, cpu) = page_address(ph);
	per_cpu(softirq_stack_ptr, cpu) = page_address(ps);

#ifdef CONFIG_L4
	l4x_stack_set((unsigned long)page_address(ph), l4x_cpu_thread_get(cpu));
	l4x_stack_set((unsigned long)page_address(ps), l4x_cpu_thread_get(cpu));
#endif /* L4 */

	return 0;
}

void do_softirq_own_stack(void)
{
#if defined(CONFIG_L4) && !defined(CONFIG_L4_VCPU)
	unsigned long cursp = (unsigned long)current_stack();
#endif
	struct irq_stack *irqstk;
	u32 *isp, *prev_esp;

	irqstk = __this_cpu_read(softirq_stack_ptr);

	/* build the stack frame on the softirq stack */
	isp = (u32 *) ((char *)irqstk + sizeof(*irqstk));

	/* Push the previous esp onto the stack */
	prev_esp = (u32 *)irqstk;
	*prev_esp = current_stack_pointer;

#if defined(CONFIG_L4) && !defined(CONFIG_L4_VCPU)
	l4x_stack_set((unsigned long)irqstk, l4_utcb());
#ifndef CONFIG_THREAD_INFO_IN_TASK
	per_cpu(l4x_current_ti, smp_processor_id()) = (struct thread_info *)irqstk;
#endif
#endif
	call_on_stack(__do_softirq, isp);
#if defined(CONFIG_L4) && !defined(CONFIG_L4_VCPU)
#ifndef CONFIG_THREAD_INFO_IN_TASK
	per_cpu(l4x_current_ti, smp_processor_id()) = (struct thread_info *)curstk;
#endif
	l4x_stack_set(cursp, l4_utcb());
#endif
}

void __handle_irq(struct irq_desc *desc, struct pt_regs *regs)
{
	int overflow = check_stack_overflow();

	if (!l4x_is_vcpu() ||
	    (user_mode(regs) || !execute_on_irq_stack(overflow, desc))) {
		if (unlikely(overflow))
			print_stack_overflow();
		generic_handle_irq_desc(desc);
	}
}
