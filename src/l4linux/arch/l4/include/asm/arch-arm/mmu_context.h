/*
 *  arch/arm/include/asm/mmu_context.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   27-06-1996	RMK	Created
 */
#ifndef __ASM_ARM_MMU_CONTEXT_H
#define __ASM_ARM_MMU_CONTEXT_H

#include <linux/compiler.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/preempt.h>

#include <asm/cacheflush.h>
#include <asm/cachetype.h>
#include <asm/proc-fns.h>
#include <asm/smp_plat.h>
#ifndef CONFIG_L4
#include <asm-generic/mm_hooks.h>
#else
#include <asm/generic/mmu_context.h>
#endif /* L4 */

void __check_vmalloc_seq(struct mm_struct *mm);

#ifdef CONFIG_CPU_HAS_ASID

void check_and_switch_context(struct mm_struct *mm, struct task_struct *tsk);
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
#ifdef CONFIG_L4
	l4x_init_new_context(tsk, mm);
#endif /* L4 */
	atomic64_set(&mm->context.id, 0);
	return 0;
}

/* L4: #ifdef CONFIG_ARM_ERRATA_798181*/
#ifdef CONFIG_L4
void a15_erratum_get_cpumask(int this_cpu, struct mm_struct *mm,
			     cpumask_t *mask);
#else  /* !CONFIG_ARM_ERRATA_798181 */
static inline void a15_erratum_get_cpumask(int this_cpu, struct mm_struct *mm,
					   cpumask_t *mask)
{
}
#endif /* CONFIG_ARM_ERRATA_798181 */

#else	/* !CONFIG_CPU_HAS_ASID */

#ifdef CONFIG_MMU

static inline void check_and_switch_context(struct mm_struct *mm,
					    struct task_struct *tsk)
{
	if (unlikely(mm->context.vmalloc_seq != init_mm.context.vmalloc_seq))
		__check_vmalloc_seq(mm);

	if (irqs_disabled())
		/*
		 * cpu_switch_mm() needs to flush the VIVT caches. To avoid
		 * high interrupt latencies, defer the call and continue
		 * running with the old mm. Since we only support UP systems
		 * on non-ASID CPUs, the old mm will remain valid until the
		 * finish_arch_post_lock_switch() call.
		 */
		mm->context.switch_pending = 1;
	else
		cpu_switch_mm(mm->pgd, mm);
}

#ifndef MODULE
#define finish_arch_post_lock_switch \
	finish_arch_post_lock_switch
static inline void finish_arch_post_lock_switch(void)
{
	struct mm_struct *mm = current->mm;

	if (mm && mm->context.switch_pending) {
		/*
		 * Preemption must be disabled during cpu_switch_mm() as we
		 * have some stateful cache flush implementations. Check
		 * switch_pending again in case we were preempted and the
		 * switch to this mm was already done.
		 */
		preempt_disable();
		if (mm->context.switch_pending) {
			mm->context.switch_pending = 0;
			cpu_switch_mm(mm->pgd, mm);
		}
		preempt_enable_no_resched();
	}
}
#endif /* !MODULE */

#endif	/* CONFIG_MMU */

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
#ifdef CONFIG_L4
	l4x_init_new_context(tsk, mm);
#endif /* L4 */
	return 0;
}


#endif	/* CONFIG_CPU_HAS_ASID */


#ifdef CONFIG_L4
#define destroy_context(mm)		l4x_destroy_context(mm)
#else
#define destroy_context(mm)		do { } while(0)
#endif
#define activate_mm(prev,next)		switch_mm(prev, next, NULL)

/*
 * This is called when "tsk" is about to enter lazy TLB mode.
 *
 * mm:  describes the currently active mm context
 * tsk: task which is entering lazy tlb
 * cpu: cpu number which is entering lazy tlb
 *
 * tsk->mm will be NULL
 */
static inline void
enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

/*
 * This is the actual mm switch as far as the scheduler
 * is concerned.  No registers are touched.  We avoid
 * calling the CPU specific function when the mm hasn't
 * actually changed.
 */
static inline void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk)
{
#ifdef CONFIG_MMU
	unsigned int cpu = smp_processor_id();

	/*
	 * __sync_icache_dcache doesn't broadcast the I-cache invalidation,
	 * so check for possible thread migration and invalidate the I-cache
	 * if we're new to this CPU.
	 */
	if (cache_ops_need_broadcast() &&
	    !cpumask_empty(mm_cpumask(next)) &&
	    !cpumask_test_cpu(cpu, mm_cpumask(next)))
		__flush_icache_all();

	if (!cpumask_test_and_set_cpu(cpu, mm_cpumask(next)) || prev != next) {
		check_and_switch_context(next, tsk);
		if (cache_is_vivt())
			cpumask_clear_cpu(cpu, mm_cpumask(prev));
	}
#endif
}

#define deactivate_mm(tsk,mm)	do { } while (0)

/* This is a (modified) copy of asm-generic/mm_hooks.h */

static inline void arch_exit_mmap(struct mm_struct *mm)
{
	mm->context.l4x_unmap_mode = L4X_UNMAP_MODE_SKIP;
}

static inline int arch_dup_mmap(struct mm_struct *oldmm,
				struct mm_struct *mm)
{
	return 0;
}

static inline void arch_unmap(struct mm_struct *mm,
                        struct vm_area_struct *vma,
                        unsigned long start, unsigned long end)
{
}

static inline void arch_bprm_mm_init(struct mm_struct *mm,
                                     struct vm_area_struct *vma)
{
}

static inline bool arch_vma_access_permitted(struct vm_area_struct *vma,
		bool write, bool execute, bool foreign)
{
	/* by default, allow everything */
	return true;
}

static inline bool arch_pte_access_permitted(pte_t pte, bool write)
{
	/* by default, allow everything */
	return true;
}

#endif
