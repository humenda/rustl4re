/**
 * \internal
 * \file   ferret/include/sensors/__llist.h
 * \brief  Functions to operate on event locked lists.
 *
 * \date   2007-05-16
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#if FERRET_LLIST_MAGIC != ahK6eeNa
#error Do not directly include this file, use a proper wrapper!
#endif
#undef FERRET_LLIST_MAGIC

#include <l4/sys/compiler.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/common.h>

#include <l4/ferret/sensors/list.h>  // we reuse some types
#include <l4/ferret/sensors/llist.h>

// reorder members such that commonly used ones are in one cacheline
typedef struct
{
    ferret_common_t      header;
    ferret_llist_index_t head;            // seq. # of el. to be written next
    uint32_t             count;           // max. # of el. (power of 2)
    //uint32_t             count_ld;        // ld of count
    uint32_t             count_mask;      // bitmask for count
    uint32_t             element_size;    // size of one element
    uint32_t             element_size_ld; // ld of size of one element
#ifdef FERRET_LLIST_HAVE_LOCK
    PREFIX(lock_t)       lock;            // global part of the lock
#endif
    uint32_t             flags;
    // data container, align to probable cache line size
    uint8_t data[0] __attribute__ ((aligned (FERRET_ASSUMED_CACHE_LINE_SIZE)));
} PREFIX(t);
