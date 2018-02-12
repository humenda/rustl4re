/**
 * \file   ferret/include/sensors/vlist_consumer.h
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
#ifndef __FERRET_INCLUDE_SENSORS_VLIST_CONSUMER_H_
#define __FERRET_INCLUDE_SENSORS_VLIST_CONSUMER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/vlist.h>

EXTERN_C_BEGIN

/* Consumer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t              header;    // copy of the header, all
                                            // sensors should start with
                                            // same header
    ferret_vlist_index_t         next_read; // event to read next
    uint64_t                     lost;      // number of events lost so far
    ferret_alist_t             * glob;      // pointer to globally shared area
    ferret_list_entry_common_t * data;      // pointer to element buffer
} ferret_alist_moni_t;

/**
 * @brief 
 *
 * @param  list list sensor to get elements from
 * @param  size size of el
 * @param  el   pointer to memory area to copy retrieved element to
 *
 * @param  size number of bytes actually written to el
 * @return - = 0 ok
 *         - < 0 errorcode
 */
int ferret_vlist_get(ferret_vlist_moni_t * list, size_t * size,
                     ferret_list_entry_t * el);

/*

++ wrap all 64 bit reads into hlh reads


retry:
    uint64_t current_tail = (volatile uint64_t)(list->glob->tail);
    if (current_tail > old_index->value)   // did we loose stuff?
    {
        list->lost += current_tail - old_index->value;
        old_index->value = list->tail.value;   // adapt
    }

retry2:
    esize = *((size_t *)(list->data[old_index->l & list->size_mask]));
    if (old_index->l & list->size_mask + esize > list->size)  // illegal size
    {
        uint64_t current_tail = (volatile uint64_t)(list->glob->tail);
        if (current_tail > old_index->value)  // temp. of perm. problem?
            goto retry;   // retry
        else
            return -1;     // sensor corrupt
    }
    memcpy(dest,
           &(list->data) + (old_index->l & list->size_mask) + sizeof(size_t),
           MIN(*size, esize));

    uint64_t current_tail = (volatile uint64_t)(list->glob->tail);
    if (list->tail.value > old_index->value)  // did we loose stuff?
    {
        old_index->value = list->tail.value;   // adapt
        goto retry2;
    }

    *size = MIN(size, esize);
 */


/**
 * @brief Setup local struct for sensor
 *
 * @param  addr pointer pointer to global memory area
 * @retval addr pointer to new local area
 */
void ferret_vlist_init_consumer(void ** addr);

/**
 * @brief De-allocate locale memory area
 *
 * @param addr  pointer pointer to local memory area
 * @retval addr pointer to global memory
 */
void ferret_vlist_free_consumer(void ** addr);

EXTERN_C_END

#endif
