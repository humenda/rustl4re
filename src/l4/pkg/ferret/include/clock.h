/**
 * \file   ferret/include/clock.h
 * \brief  macros and defines for getting time values
 *
 * \date   04/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universit채t Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_CLOCK_H_
#define __FERRET_INCLUDE_CLOCK_H_

#include <l4/ferret/types.h>
#include <l4/re/env.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/rdtsc.h>
#endif

#include <l4/sys/compiler.h>
#include <l4/sys/thread.h>

/*********************************************************************
 * Time sources
 *********************************************************************/

// reserve 0 as illegal value !

enum TimeSource
{
	/*
	 * Reserved
	 */
	FERRET_ILLEGAL_TIME = 0,
	/*
	 * Directly use CPU ticks. Absolute time since bootup.
	 */
	FERRET_TIME_ABS_TSC = 1,
	/*
	 * Same as FERRET_TIME_ABS_TSC, but convert ticks to 탎 via l4_tsc_to_us().
	 * Absolute time since booutp.
	 */
	FERRET_TIME_ABS_US  = 2,
	/*
	 * Accumulated time in 탎, relatvie to thread.
	 * THIS IS SLOW!
	 */
	FERRET_TIME_REL_US  = 3,
#if 0
	/*
	 * Accumulated time in ticks, relatvie to thread.
	 * THIS IS NOT FAST IN RE!
	 */
	FERRET_TIME_REL_TSC_FAST = 4,
	/*
	 * Accumulated time in 탎, relatvie to thread.
	 * THIS IS NOT FAST IN RE!
	 */
	FERRET_TIME_REL_US_FAST = 5,
#endif
};



/*********************************************************************
 * Time macros
 *********************************************************************/

// fixme: sort case block according to common cases

EXTERN_C ferret_time_t ferret_rel_utime(void);

#define FERRET_GET_TIME(clock_type, time_val)                             \
do                                                                        \
{                                                                         \
	switch (clock_type)                                                   \
	{                                                                     \
	case FERRET_TIME_REL_US:                                              \
		(time_val) = ferret_rel_utime();                                  \
		break;                                                            \
	case FERRET_TIME_ABS_TSC:                                             \
		(time_val) = l4_rdtsc();                                          \
		break;                                                            \
	case FERRET_TIME_ABS_US:                                              \
		(time_val) = l4_tsc_to_us(l4_rdtsc());                            \
		break;                                                            \
	default:                                                              \
		printf("corrupted CLOCKTYPE\n");                                  \
	}                                                                     \
} while (0)


/**
 * @brief Calibrates scalers etc. for the use of the corresponding clock
 *
 * @param clock clock type to calibrate for
 */
L4_INLINE L4_CV void ferret_calibrate_clock(int clock_type);
L4_INLINE L4_CV void ferret_calibrate_clock(int clock_type)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
	if (clock_type == FERRET_TIME_REL_US)
		if (l4_scaler_tsc_to_us == 0)  // calibrate on demand
			l4_calibrate_tsc(l4re_kip());
#else
	(void)clock_type;
#endif
}

#endif
