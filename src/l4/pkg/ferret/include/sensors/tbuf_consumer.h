/**
 * \file   ferret/include/sensors/tbuf_consumer.h
 * \brief  Tracebuffer consumer functions.
 *
 * \date   06/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_TBUF_CONSUMER_H_
#define __FERRET_INCLUDE_SENSORS_TBUF_CONSUMER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/list.h>

#include <l4/sys/ktrace.h>
#include <l4/sys/ktrace_events.h>

EXTERN_C_BEGIN

/* Also the tracebuffer sensor has two parts, the kernel-provided
 * global part and the process-local status memory.  The tracebuffer
 * cannot be transported with dataspaces but must be allocated
 * directly at the kernel (I assume).
 *
 * The shared part is described in <l4/sys/ktrace.h> and
 * <l4/sys/ktrace_events.h>, the local part in "tbuf_consumer.h"
 */

/* Consumer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t           header;    // copy of the header, all
                                         // sensors should start with
                                         // same header
    uint64_t                  next_read; // event to read next
    uint64_t                  lost;      // number of events lost so far
    l4_tracebuffer_status_t * status;    // pointer to globally shared area
    int                       size;      // size of tracebuffer in bytes
    int                       count;     // number of tracebuffer entries
    int                       count2;    // number of tracebuffer entries / 2
    uint32_t                  mask;      // bitmask to create index
                                         // from counter
    uint32_t                  offset;    // how many bits offset for
                                         // version[01] to combine
                                         // with event number ->
                                         // element counter
} ferret_tbuf_moni_t;

/**
 * @brief Get one event out of the tracebuffer
 *
 * @param  list list sensor to get elements from
 * @param  el   pointer to memory area to copy retrieved element to
 *
 * @return - = 0 ok
 *         - < 0 errorcode
 */
int ferret_tbuf_get(ferret_tbuf_moni_t * tbuf, l4_tracebuffer_entry_t * el);

/**
 * @brief Setup local struct for sensor
 *
 * @param  addr pointer pointer to global memory area
 * @retval addr pointer to new local area
 */
void ferret_tbuf_init_consumer(void ** addr);

/**
 * @brief De-allocate locale memory area
 *
 * @param addr  pointer pointer to local memory area
 * @retval addr pointer to global memory
 */
void ferret_tbuf_free_consumer(void ** addr);

EXTERN_C_END

#endif
