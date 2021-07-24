/*
 *  linux/arch/arm/mach-realview/hotplug.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/smp.h>

#include <asm/generic/smp.h>
#include <asm/generic/tamed.h>
#include <l4/util/util.h>

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void l4x_cpu_die(unsigned int cpu)
{
#ifndef CONFIG_L4_VCPU
	l4x_tamed_shutdown(cpu);
#endif
	l4x_shutdown_cpu(cpu);
	printk("CPU%d still alive...\n", raw_smp_processor_id());
	l4_sleep_forever();
}
