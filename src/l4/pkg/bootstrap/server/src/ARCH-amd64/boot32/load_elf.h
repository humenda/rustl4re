/*
 * (c) 2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef LOAD_ELF_H__
#define LOAD_ELF_H__ 1

#include "types.h"

typedef void (*Startup_func)(void);

l4_uint32_t load_elf(void *addr, l4_uint32_t *vma_start, l4_uint32_t *vma_end);

#endif
