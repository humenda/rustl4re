/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef BOOT_CPU_H
#define BOOT_CPU_H

#include "types.h"

void base_paging_init (l4_uint64_t);
void base_cpu_setup (void);

extern struct boot32_info_t boot32_info;

#endif
