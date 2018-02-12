/**
 * \file   ferret/include/sensors/list_producer.h
 * \brief  List producer functions.
 *
 * \date   22/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_LIST_PRODUCER_H_
#define __FERRET_INCLUDE_SENSORS_LIST_PRODUCER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

/* Producer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t       header;      // copy of the header, all
                                       // sensors should start with
                                       // same header
    ferret_list_t       * glob;        // pointer to globally shared area
    ferret_list_index_t * ind_buf;     // pointer to index entry buffer
    void                * out_buf;     // pointer to output element buffer
} ferret_list_local_t;

/* Two interface levels:
 *  - Low level
 *     - dequeue element
 *     - get element for index
 *     - enqueue element
 *  - High level (see list_producer_wrapper.h)
 *     - Write an element of size n
 */

/**
 * @brief Dequeues one element from the list sensor
 *
 * @param list list sensor to work on
 *
 * @return index to dequeued output element
 */
L4_INLINE int ferret_list_dequeue(ferret_list_local_t * list);

/**
 * @brief Commit an previously dequeue element back to the shared
 *        buffer
 *
 * @param list list sensor to work on
 * @param index index of the element to commit
 */
L4_INLINE void ferret_list_commit(ferret_list_local_t * list, int index);

/**
 * @brief Setup local struct for sensor
 *
 * @param  addr  pointer pointer to global memory area
 * @param  alloc function pointer to memory allocating function
 * @retval addr  pointer to new local area
 */
void ferret_list_init_producer(void ** addr, void *(*alloc)(size_t size));

/**
 * @brief De-allocate local memory area
 *
 * @param  addr pointer pointer to local memory area
 * @param  free function pointer to memory freeing function
 * @retval addr pointer to referenced global memory
 */
void ferret_list_free_producer(void ** addr, void (*free)(void *));


/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>
#include <l4/util/rdtsc.h>

L4_INLINE int ferret_list_dequeue(ferret_list_local_t * list)
{
    ferret_list_index_t tail, new_tail;
    int index;

    while (1)
    {
        // read element counter atomically (high, low, high; with retry)
        tail.lh.high = list->glob->tail.lh.high;
        tail.lh.low  = list->glob->tail.lh.low;
        if (L4_UNLIKELY(tail.lh.high != list->glob->tail.lh.high))
            continue;

        index = list->ind_buf[tail.parts.index].parts.index;

///*        new_tail.value = tail.value;
//        new_tail.parts.elements++;*/
//        new_tail.value = tail.value + (1 << FERRET_INDEX_BITS);
//        new_tail.lh.low++;

        // fixme: what is faster?
        // increase element counter and index
        new_tail.value = tail.value;
        ++(new_tail.parts.element);
        ++(new_tail.parts.index);

        // care for index wrap-around
        if (L4_UNLIKELY(new_tail.parts.index >= list->glob->count))
            new_tail.parts.index = 0;

        // finally try to dequeue element, or start from scratch
        if (L4_LIKELY(l4util_cmpxchg64(&list->glob->tail.value,
                                       tail.value, new_tail.value)))
            return index;
    }
}

L4_INLINE void ferret_list_commit(ferret_list_local_t * list, int index)
{
    ferret_list_index_t head, new_head, head_index, new_head_index;
    volatile ferret_list_index_t * head_pos;  // points to shared region

    // compute the output element where index points to
    ferret_list_entry_t * oe = (ferret_list_entry_t *)
        (((char *)(list->out_buf)) + index * list->glob->element_size);

    int ret = 0;

    while (1)
    {
        // read element counter atomically (high, low, high; with retry)
        head.lh.high = list->glob->head.lh.high;
        head.lh.low  = list->glob->head.lh.low;
        if (L4_UNLIKELY(head.lh.high != list->glob->head.lh.high))
            continue;

        // read the index element head points to
        head_pos = &(list->ind_buf[head.parts.index]);
        head_index.lh.high = head_pos->lh.high;
        head_index.lh.low  = head_pos->lh.low;
        // retry if head_index changed during read or element has been
        // written to (its elements tag is too high, the EC read is really
        // out-dated)
        if (L4_UNLIKELY(head_index.lh.high != head_pos->lh.high ||
                         head_index.parts.element > head.parts.element))
            continue;

        // If element counters are equal, then the current thread is similar
        // to the one that failed at the first cmpxchg.  Restarting the loop
        // would result in a dead-lock (if no other thread increases the EC).
        // Instead, the current thread must try to increase the EC as well.
        // If the index' EC is smaller, we can try to enqueue our OE.
        if (L4_LIKELY(head_index.parts.element < head.parts.element))
        {
            // Set the timestamp to now.  This is done in the loop since we
            // want to keep timestamps ordered.  The extra costs of rdtsc
            // are small compared to the costs of two cmpxchg8b.
            oe->timestamp = l4_rdtsc();

            // try to exchange head_index

// do we need this line ???  The value is overwritten in the next line ...
//            new_head_index.value          = head_index.value;

            new_head_index.parts.index   = index;
            new_head_index.parts.element = head.parts.element;
            ret = l4util_cmpxchg64(&head_pos->value, head_index.value,
                                   new_head_index.value);
        }

        // Try to advance head.  This is always done, even if the
        // previous cmpxchg failed, since we might need to help other writers
        // that succeeded in exchanging head_index but got preempted
        // before they were able to increase head.

      /*new_head.value = head.value + (1 << GRTMON_OE_INDEX_INDEX_BITS);
        new_head.lh.low++;
      */
        // fixme: what is faster?
        // care for wrap-around
        new_head.value = head.value;
        ++(new_head.parts.element);
        ++(new_head.parts.index);
        if (L4_UNLIKELY(new_head.parts.index >= list->glob->count))
            new_head.parts.index = 0;

        l4util_cmpxchg64(&list->glob->head.value, head.value, new_head.value);
        // if head_index has been exchanged we're done because someone
        // will have increased head
        if (ret)
            break;
    }
}

EXTERN_C_END

#endif
