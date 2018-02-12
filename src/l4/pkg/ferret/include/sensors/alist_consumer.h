/**
 * \file   ferret/include/sensors/alist_consumer.h
 * \brief  Atomic list consumer functions.
 *
 * \date   2007-09-27
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_ALIST_CONSUMER_H_
#define __FERRET_INCLUDE_SENSORS_ALIST_CONSUMER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/alist.h>

EXTERN_C_BEGIN

/* Consumer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t              header;    // copy of the header, all
                                            // sensors should start with
                                            // same header
    uint32_t                     next_read; // event to read next
    uint64_t                     lost;      // number of events lost so far
    ferret_alist_t             * glob;      // pointer to globally shared area
    ferret_list_entry_common_t * data;      // pointer to element buffer
} ferret_alist_moni_t;

/**
 * @brief 
 *
 * @param  list list sensor to get elements from
 * @param  el   pointer to memory area to copy retrieved element to
 *
 * @return - = 0 ok
 *         - < 0 errorcode
 */
int ferret_alist_get(ferret_alist_moni_t * list, ferret_list_entry_t * el);

/**
 * @brief Setup local struct for sensor
 *
 * @param  addr pointer pointer to global memory area
 * @retval addr pointer to new local area
 */
void ferret_alist_init_consumer(void ** addr);

/**
 * @brief De-allocate locale memory area
 *
 * @param addr  pointer pointer to local memory area
 * @retval addr pointer to global memory
 */
void ferret_alist_free_consumer(void ** addr);

EXTERN_C_END

#endif
