/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2020 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */

#include <stdio.h>

#include <l4/sys/consts.h>

#include "support.h"

void L4_NORETURN
panic(char const *str)
{
  printf("PANIC: %s\n", str);
  while (1)
    ;
}

typedef struct
{
  unsigned long start;
  unsigned long size;
} Area;

typedef struct
{
  unsigned num;
  Area entries[Max_reservations];
} Reservations;

static Reservations reservations;

void
reservation_add(unsigned long start, unsigned long size)
{
  unsigned long _start = l4_trunc_page(start);
  unsigned long _size  = l4_round_page(start - _start + size);
  if (_start > (1ULL << 32) - _size)
    panic("Cannot add reservation that exceeds 32-bit address space");

  for (Area *r = reservations.entries;
       r < reservations.entries + reservations.num; ++r)
    {
      // Inside range:
      if (_start >= r->start && _start + _size <= r->start + r->size)
          return;

      // Directly after
      if (r->start + r->size == _start)
        {
          r->size += _size;
          return;
        }

      // Directly before
      if (r->start == _start + _size)
        {
          r->start = _start;
          return;
        }
    }

  if (reservations.num < Max_reservations)
    {
      reservations.entries[reservations.num].start = _start;
      reservations.entries[reservations.num].size  = _size;
      ++reservations.num;
    }
  else
    {
      Area *e = reservations.entries + (Max_reservations - 1);
      printf("WARNING: Extending last reservation entry (%u) from [%lx:%lx]",
             Max_reservations - 1, e->start, e->start + e->size - 1);
      if (e->start > _start)
        e->start = _start;
      if (e->start + e->size < _start + _size)
        e->size = _start + _size - e->start;
      printf(" to [%lx:%lx]\n", e->start, e->start + e->size - 1);
    }
}

unsigned long
overlaps_reservation(void* start, unsigned long size)
{

  for (Area *r = reservations.entries;
       r < reservations.entries + reservations.num; ++r)
    {
      if (r->start < (unsigned long)start + size
          && r->start + r->size > (unsigned long)start)
        return r->start + r->size;
    }

  return 0;
}

