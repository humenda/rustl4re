/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef INIT_KIP_H__
#define INIT_KIP_H__

#include <l4/util/mb_info.h>
#include "startup.h"
#include "init_kip-arch.h"

#ifdef __cplusplus
class Region_list;

void init_kip_f(void *_l4i, boot_info_t *bi, l4util_mb_info_t *mbi,
                Region_list *ram, Region_list *regions);
#endif

#endif
