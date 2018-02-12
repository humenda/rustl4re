/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <pthread-l4.h>

ferret_time_t ferret_rel_utime()
{
	l4_cap_idx_t c = pthread_l4_cap(pthread_self());
	l4_kernel_clock_t t;
	if (l4_error(l4_thread_stats_time(c, &t)))
		return (ferret_time_t)~0ULL;
	return (ferret_time_t)t;
}
