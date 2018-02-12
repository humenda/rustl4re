/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef INIT_KIP_ARCH_H__
#define INIT_KIP_ARCH_H__

#include <l4/sys/kip.h>

#if defined(ARCH_ppc32)
void init_kip_v2_arch(l4_kernel_info_t *);
#endif

#endif

