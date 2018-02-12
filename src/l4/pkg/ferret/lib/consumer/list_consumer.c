/**
 * \file   ferret/lib/sensor/list_consumer.c
 * \brief  List consumer functions.
 *
 * \date   23/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <string.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/list_consumer.h>

int ferret_list_get(ferret_list_moni_t * list, ferret_list_entry_t * el)
{
    /* 1. read tail pointer atomically
     *  - do we still have the element requested?
     *    No -> increase seq sensibly, Yes -> Go on!
     * 2. read head pointer atomically
     *  - do we already have the element requested? No -> Return, Yes -> Go on!
     * 3. Get element (copy)
     * 4. Check if we got an valid element (if we were not overtaken by tail)
     *  - Yes -> Return valid data!
     *  - No  -> Return, that we missed it!
     */

    ferret_list_index_t head, tail;
    uint32_t index, el_ind;
    int tries;

    do    // get tail pointer
    {
        tail.lh.high = list->glob->tail.lh.high;
        tail.lh.low  = list->glob->tail.lh.low;
    } while (tail.lh.high != list->glob->tail.lh.high);

    for (tries = 0; tries < 3; ++tries)
    {
        if (tail.parts.element > list->next_read)
        {
            // fixme: maybe we should skip more ?
            list->lost += tail.parts.element - list->next_read;
            list->next_read = tail.parts.element;
        }

        do    // get head pointer
        {
            head.lh.high = list->glob->head.lh.high;
            head.lh.low  = list->glob->head.lh.low;
        } while (head.lh.high != list->glob->head.lh.high);

        if (head.parts.element > list->next_read)
        {
            // fixme: we should cache this information
            index  = list->next_read % list->glob->count;
            el_ind = list->ind_buf[index].parts.index;
            if (el_ind >= list->glob->count)
                continue;  // probably concurrent update -> inconsistent data

            memcpy(el, ferret_list_e4i(list->glob, el_ind),
                   list->glob->element_size);

            // get tail pointer again to check whether we were
            // overtaken in the last couple steps
            do
            {
                tail.lh.high = list->glob->tail.lh.high;
                tail.lh.low  = list->glob->tail.lh.low;
            } while (tail.lh.high != list->glob->tail.lh.high);
            if (tail.parts.element > list->next_read)
            {
                continue;  // probably inconsistent data, try once more
            }
            else           // consistent data, return it
            {
                ++(list->next_read);
                return 0;
            }
        }
        else
        {
            return -1;  // no new data available
        }
    }
    return -2;  // corrupt sensor
}

void ferret_list_init_consumer(void ** addr)
{
    ferret_list_moni_t * temp;

    temp = malloc(sizeof(ferret_list_moni_t));

    // setup some pointers
    temp->glob    = *addr;  // store pointer to global structure
    temp->ind_buf = (ferret_list_index_t *)(temp->glob->data);
    temp->out_buf = temp->glob->data +
        sizeof(ferret_list_index_t) * temp->glob->count;

    // copy header
    temp->header.major    = temp->glob->header.major;
    temp->header.minor    = temp->glob->header.minor;
    temp->header.instance = temp->glob->header.instance;
    temp->header.type     = temp->glob->header.type;

    // init. local information
    temp->lost      = 0;
    temp->next_read = temp->glob->count;

    *addr = temp;
}

void ferret_list_free_consumer(void ** addr)
{
    void * temp = ((ferret_list_moni_t *)(*addr))->glob;

    free(*addr);

    *addr = temp;
}
