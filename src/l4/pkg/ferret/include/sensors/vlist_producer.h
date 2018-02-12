/**
 * \file   ferret/include/sensors/vlist_producer.h
 * \brief  Variable list producer functions.
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
#ifndef __FERRET_INCLUDE_SENSORS_VLIST_PRODUCER_H_
#define __FERRET_INCLUDE_SENSORS_VLIST_PRODUCER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/vlist.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

L4_INLINE void
ferret_vlist_post(ferret_vlist_t * l, uint16_t maj, uint16_t min,
                  uint16_t instance);

L4_INLINE void
ferret_vlist_post_2w(ferret_vlist_t * l, uint16_t maj, uint16_t min,
                     uint16_t instance, uint32_t d0, uint32_t d1);

/* Post an event of variable size, size and pointer to payload must be
 * given.
 */
L4_INLINE void
ferret_vlist_post_v(ferret_vlist_t * l, uint16_t maj, uint16_t min,
                    uint16_t instance, size_t size, void * p);

/* Post an event of variable size in raw format, only timestamp is
 * prepended on post, size and pointer to payload must be given.
 */
L4_INLINE void
ferret_vlist_post_v(ferret_vlist_t * l, size_t size, void * p);

/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>
#include <l4/util/rdtsc.h>

L4_INLINE void
ferret_vlist_post(ferret_vlist_t * l, uint16_t maj, uint16_t min,
                  uint16_t instance)
{
    ferret_list_entry_common_t e;

    e.major     = maj;
    e.minor     = min;
    e.instance  = instance;

    // fixme: call into real post event code in kern_lib_page

    /*
    asm volatile (
        "cld                              \n\t"
        "push %%ecx                       \n\t"
        "push %%esi                       \n\t"
        "push %%eax                       \n\t"
        "call __ferret_alist_post         \n\t"
        "add  $0xc, %%esp                 \n\t"
        :
        : "S"((&(e.timestamp)) + 1), "a"(l), "c"(2)
        : "memory", "edi", "ebx", "edx", "cc"
        );
    */
}

/*
// write:
// parameters: size. *p
// fixme: care for wrap-around in memcpy
    retry:
    while (((l->head + l->size) - (l->tail.low & l->size_mask)) &
           l->size_mask <
           size + sizeof(size_t))
    {
        new_tail = l->tail.value + sizeof(size_t) + size;
        cmpxchg8b(l->tail.value, new_tail, &l->tail.value);  // We are
            //  executed atomically, so we don't have to store
            //  l->tail.value somewhere else.  This is basically just
            //  an atomic 64-bit move.
    }

    // we fill the remainder of the buffer with a pseudo event, in
    // case of wrap-around.
    //  - small events (< 12 bytes) are such events
    //  - larger events with a timestamp of 0 too

    // we could also split the memcpy ???

    free = l->size - l->head;
    assert(free >= sizeof(size_t));
    if (free < size + sizeof(size_t))
    {
        dest = l->head + &(l->data);
        *dest = free - sizeof(size_t);
        if (free >= sizeof(size_t) + sizeof(ferret_utime_t))
            (ferret_utime_t *)(dest + 2) = 0;  // 0 means 
        goto retry:
    }

    *p = rdtsc();
    *(l->head.low & l->size_mask) + &(l->data) = size;
    memcpy((l->head.low & l->size_mask) + &(l->data) + sizeof(size_t),
           p, size);

    new_head = (l->head + sizeof(size_t) + size) & l->size_mask;
    // rollforward point
    l->head = new_head;
 */

/*
// read:
// parameters: *old_index (64 bit), *dest
// fixme: we can not know how many events we lost, but just about how
//        many bytes ...

retry:
    if (l->tail.value > old_index->value)   // did we loose stuff?
        old_index->value = l->tail.value;   // adapt

retry2:
    size = *(&(l->data) + (old_index->l & l->size_mask));
    if (old_index->l + size > l->size)  // got illegal size
        if (l->tail.value > old_index->value)  // temp. of permament problem?
            goto retry;   // retry
        else
            return 1;     // sensor corrupt
    memcpy(dest,
           &(l->data) + (old_index->l & l->size_mask) + sizeof(size_t),
           size);
    if (l->tail.value > old_index->value)  // did we loose stuff?
        old_index->value = l->tail.value;   // adapt
        goto retry2;
 */

EXTERN_C_END

#endif
