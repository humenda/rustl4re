/*
 *  linux/arch/arm/mach-realview/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>

#include <asm/generic/smp.h>

#include <l4/util/util.h>

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
#ifndef CONFIG_L4
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
#endif
}

#ifndef CONFIG_L4
static void __iomem *scu_base_addr(void)
{
	if (machine_is_realview_eb_mp())
		return __io_address(REALVIEW_EB11MP_SCU_BASE);
	else if (machine_is_realview_pb11mp())
		return __io_address(REALVIEW_TC11MP_SCU_BASE);
	else if (machine_is_realview_pbx() &&
		 (core_tile_pbx11mp() || core_tile_pbxa9mp()))
		return __io_address(REALVIEW_PBX_TILE_SCU_BASE);
	else
		return (void __iomem *)0;
}
#endif

static DEFINE_SPINLOCK(boot_lock);

static void l4x_smp_secondary_init(unsigned int cpu)
{
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
#ifndef CONFIG_L4
	gic_secondary_init(0);
#endif

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int l4x_smp_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu);

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
#ifdef CONFIG_L4
	l4x_cpu_release(cpu);
#else
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));
#endif

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

#ifdef CONFIG_L4
		L4XV_FN_v(l4_sleep(10));
#else
		udelay(10);
#endif
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

#ifdef CONFIG_L4
	return 0;
#else
	pen_release != -1 ? -ENOSYS : 0;
#endif
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
static void __init l4x_smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = l4x_nr_cpus;

	/* sanity check */
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(l4x_raise_softirq);
}

static void __init l4x_smp_prepare_cpus(unsigned int max_cpus)
{
}

// from hotplug.c
void l4x_cpu_die(unsigned int cpu);

const struct smp_operations l4x_smp_ops __initconst = {
	.smp_init_cpus		= l4x_smp_init_cpus,
	.smp_prepare_cpus       = l4x_smp_prepare_cpus,
	.smp_secondary_init	= l4x_smp_secondary_init,
	.smp_boot_secondary	= l4x_smp_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= l4x_cpu_die,
#endif
};
