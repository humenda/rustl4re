/**
 * \file   ferret/lib/sensor/__llist_consumer.c
 * \brief  locked list consumer functions.
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
#error Do not directly include this file, use a propper wrapper!
#endif
#undef FERRET_LLIST_MAGIC

#include <stdlib.h>

// fixme: debug
//#include <l4/log/l4log.h>

#include <l4/ferret/types.h>

#include <l4/sys/ipc.h>

// fixme: maybe later we need functions to get more than 1 element out
//        of the buffer, thereby saving some locking overhead
int PREFIX(get)(PREFIX(moni_t) * list, ferret_list_entry_t * el)
{
    uint32_t index;

    /* 1. lock list
     * 2. check if new data is available (no -> return -1)
     * 3. check if we lost something and update lost counter
     * 4. compute offset in locked list's data array
     * 5. copy data
     * 6. lock list
     */

    FERRET_LLIST_LOCAL_LOCK(list);

    // check if we have new events for the consumer
    if (L4_UNLIKELY(list->glob->head.value <= list->next_read.value))
        return -1;  // we have no new events
    // check if we have lost events
    if (L4_UNLIKELY(list->glob->head.value - list->next_read.value >
                    list->glob->count))
    {
        list->lost += list->glob->head.value - list->next_read.value -
                      list->glob->count;
        list->next_read.value = list->glob->head.value - list->glob->count;
    }
    index = list->next_read.lh.low & list->glob->count_mask;
    memcpy(el, list->out_buf + (index << list->glob->element_size_ld),
           list->glob->element_size);

    list->next_read.value++;
    FERRET_LLIST_LOCAL_UNLOCK(list);
    return 0;

    //return -2;  // corrupt sensor
}

void PREFIX(init_consumer)(void ** addr)
{
    PREFIX(moni_t) * temp;

    temp = malloc(sizeof(PREFIX(moni_t)));

    // setup some pointers
    temp->glob    = *addr;  // store pointer to global structure
    temp->out_buf = temp->glob->data;

    // copy header
    temp->header.major    = temp->glob->header.major;
    temp->header.minor    = temp->glob->header.minor;
    temp->header.instance = temp->glob->header.instance;
    temp->header.type     = temp->glob->header.type;

    // init. local information
    temp->lost            = 0;
    temp->next_read.value = 0;

#ifdef FERRET_LLIST_HAVE_LOCAL_LOCK
    FERRET_LLIST_LOCAL_INIT(temp);
#endif

    *addr = temp;
}

void PREFIX(free_consumer)(void ** addr)
{
    void * temp = ((PREFIX(moni_t) *)(*addr))->glob;

    free(*addr);

    *addr = temp;
}
