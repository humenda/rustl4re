/**
 * \file   ferret/include/sensors/alist.h
 * \brief  Functions to operate on atomic event lists.
 *
 * \date   2007-07-23
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_ALIST_H_
#define __FERRET_INCLUDE_SENSORS_ALIST_H_

#include <l4/sys/compiler.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/common.h>


#define FERRET_ALIST_ELEMENT_SIZE    64
#define FERRET_ALIST_ELEMENT_SIZE_LD 6

#define FERRET_ASSUMED_CACHE_LINE_SIZE 64

#define FERRET_ALIST_MAX_COUNT   16*1024*1024  // arbitrary, but large limit

#include <l4/ferret/sensors/list.h>  // reuse some types

typedef struct
{
    ferret_common_t header;

    uint32_t head;
    uint32_t count;          // max. number of elements
    uint32_t count_ld;       // log_2(number of elements)
    uint32_t count_mask;     // (2 << count_ld) - 1
    uint32_t flags;
    // data container, align to probable cache line size
    ferret_list_entry_common_t data[0]
      __attribute__ ((aligned (FERRET_ASSUMED_CACHE_LINE_SIZE)));
} ferret_alist_t;

#endif
