/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <linux/cpumask.h>

#include "local.h"

static struct cpumask _possible = CPU_MASK_ALL;
static struct cpumask _online   = CPU_MASK_CPU0;
static struct cpumask _present  = CPU_MASK_CPU0;
static struct cpumask _active   = CPU_MASK_CPU0;

const struct cpumask *const cpu_possible_mask = &_possible;
const struct cpumask *const cpu_online_mask   = &_online;
const struct cpumask *const cpu_present_mask  = &_present;
const struct cpumask *const cpu_active_mask   = &_active;

cpumask_t cpu_mask_all = CPU_MASK_ALL;
int nr_cpu_ids = NR_CPUS;
const DECLARE_BITMAP(cpu_all_bits, NR_CPUS);

/* cpu_bit_bitmap[0] is empty - so we can back into it */
#define MASK_DECLARE_1(x)   [x+1][0] = 1UL << (x)
#define MASK_DECLARE_2(x)   MASK_DECLARE_1(x), MASK_DECLARE_1(x+1)
#define MASK_DECLARE_4(x)   MASK_DECLARE_2(x), MASK_DECLARE_2(x+2)
#define MASK_DECLARE_8(x)   MASK_DECLARE_4(x), MASK_DECLARE_4(x+4)

const unsigned long cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(NR_CPUS)] = { 
    MASK_DECLARE_8(0),  MASK_DECLARE_8(8),
    MASK_DECLARE_8(16), MASK_DECLARE_8(24),
#if BITS_PER_LONG > 32
    MASK_DECLARE_8(32), MASK_DECLARE_8(40),
    MASK_DECLARE_8(48), MASK_DECLARE_8(56),
#endif
};

void __smp_call_function_single(int cpuid, struct call_single_data *data)
{
	data->func(data->info);
}
