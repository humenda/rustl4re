/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef BOOT_PAGING_H
#define BOOT_PAGING_H

#include "types.h"

enum
{
  PAGE_SIZE 			= (1 << 12),
  PAGE_MASK			= (PAGE_SIZE - 1),
  SUPERPAGE_SIZE		= (1 << 21),
  SUPERPAGE_MASK		= (SUPERPAGE_SIZE - 1),
  PD_SIZE			= (1 << 30),
  PD_MASK			= (PD_SIZE - 1),
  PDP_SIZE			= (1LL << 39),
  PDP_MASK			= (PDP_SIZE - 1),
};

static inline int
superpage_aligned(l4_uint32_t x)
{ return (x & SUPERPAGE_MASK) == 0; }

static inline int
pd_aligned(l4_uint32_t x)
{ return (x & PD_MASK) == 0; }

static inline int
pdp_aligned(l4_uint32_t x)
{ return (x & PDP_MASK) == 0; }

static inline l4_uint64_t round_superpage(l4_uint64_t x)
{ return (x + SUPERPAGE_MASK) & ~SUPERPAGE_MASK; }

#endif
