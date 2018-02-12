/*
 *  arch/arm/include/asm/timex.h
 *
 *  Copyright (C) 1997,1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Architecture Specific TIME specifications
 */
#ifndef _ASMARM_TIMEX_H
#define _ASMARM_TIMEX_H

#ifndef DDE_LINUX
#include <mach/timex.h>
#else
#define CLOCK_TICK_RATE		(50000000 / 16)
#endif

typedef unsigned long cycles_t;

static inline cycles_t get_cycles (void)
{
	return 0;
}

#endif
