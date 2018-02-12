/*****************************************************************************/
/**
 * \file   input/lib/src/emul_time.c
 * \brief  L4INPUT: Linux time emulation
 *
 * \date   2007-05-29
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2007-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* L4 */
#if defined(ARCH_x86) || defined(ARCH_amd64) || defined(ARCH_ppc32)
#include <l4/util/rdtsc.h>     /* XXX x86 specific */
#endif
#include <l4/util/util.h>

/* Linux */
#include <asm/delay.h>

/* UDELAY */
void udelay(unsigned long usecs)
{
#ifdef ARCH_arm
  l4_sleep(usecs/1000); // XXX
#else
  l4_busy_wait_us(usecs);
#endif
}
