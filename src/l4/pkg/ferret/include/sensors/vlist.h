/**
 * \file   ferret/include/sensors/vlist.h
 * \brief  Functions to operate on variable event lists.
 *
 * \date   2007-09-26
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_VLIST_H_
#define __FERRET_INCLUDE_SENSORS_VLIST_H_

#include <l4/sys/compiler.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/common.h>


#define FERRET_ASSUMED_CACHE_LINE_SIZE 64

#define FERRET_VLIST_MAX_SIZE   64*1024*1024  // arbitrary, but large limit

#include <l4/ferret/sensors/list.h>  // reuse some types

typedef union ferret_vlist_index_s
{
    struct
    {
        uint32_t low;
        uint32_t high;
    };
    uint64_t     value;
} ferret_vlist_index_t;

typedef struct
{
    ferret_common_t      header;

    uint32_t             head;       // maybe make 64-bit too?
    ferret_vlist_index_t tail;
    uint32_t             size;       // size of sensor data in bytes (must
                                     // be power of two)
    uint32_t             size_ld;    // ld_2(size)
    uint32_t             size_mask;  // (2 << size_ld) - 1
    uint32_t             flags;

    uint8_t              data[0]     // data, align to cache line size
      __attribute__ ((aligned (FERRET_ASSUMED_CACHE_LINE_SIZE)));
} ferret_alist_t;

#endif
