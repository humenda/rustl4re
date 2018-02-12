/*
 * cpu_accel.c
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdint.h>
#include <l4/util/cpu.h>
#include "mm_accel.h"

static uint32_t x86_accel (void)
{
    unsigned long eax, ebx, ecx, edx;
    int AMD;
    uint32_t caps;

    if (!l4util_cpu_has_cpuid())
	return 0;

    l4util_cpu_cpuid (0x00000000, &eax, &ebx, &ecx, &edx);
    if (!eax)			// vendor string only
	return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    l4util_cpu_cpuid (0x00000001, &eax, &ebx, &ecx, &edx);
    if (! (edx & 0x00800000))	// no MMX
	return 0;

    caps = MM_ACCEL_X86_MMX;
    if (edx & 0x02000000)	// SSE - identical to AMD MMX extensions
	caps = MM_ACCEL_X86_MMX | MM_ACCEL_X86_MMXEXT;

    l4util_cpu_cpuid (0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001)	// no extended capabilities
	return caps;

    l4util_cpu_cpuid (0x80000001, &eax, &ebx, &ecx, &edx);

    if (edx & 0x80000000)
	caps |= MM_ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))	// AMD MMX extensions
	caps |= MM_ACCEL_X86_MMXEXT;

    return caps;
}

uint32_t mm_accel (void)
{
    static int got_accel = 0;
    static uint32_t accel;

    if (!got_accel) {
	got_accel = 1;
	accel = x86_accel ();
    }

    return accel;
}
