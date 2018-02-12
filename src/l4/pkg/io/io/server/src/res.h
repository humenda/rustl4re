/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/l4int.h>
#include <l4/sys/capability>

int res_init();

#if defined(ARCH_x86) || defined(ARCH_amd64)
int res_get_ioport(unsigned port, int size);
#endif

l4_addr_t res_map_iomem(l4_addr_t phys, l4_addr_t size);
