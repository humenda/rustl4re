/**
 * \file   ferret/lib/sensor/alist_consumer.c
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
#include <stdlib.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/alist.h>
#include <l4/ferret/sensors/alist_consumer.h>

int ferret_alist_get(ferret_alist_moni_t * list, ferret_list_entry_t * el)
{
    /*
      check if our own pointer is still valid
          if not, update it to tail
          update lost counter too

      retry:
          memcpy entry from buffer

          check if our own pointer is still valid
              if not, update it to tail
              update lost counter too
              goto retry
    */

    uint32_t current_tail = (volatile uint32_t)(list->glob->head) -
        (list->glob->count - 1);

    if (list->next_read < current_tail)  // have we lost stuff?
    {
        list->lost += current_tail - list->next_read;  // update counter
        list->next_read = current_tail;  // try oldest avail. instead
    }

retry:

    memcpy(el, &list->data[list->next_read & list->glob->count_mask],
           FERRET_ALIST_ELEMENT_SIZE);

    current_tail = (volatile uint32_t)(list->glob->head) -
        (list->glob->count - 1);

    if (list->next_read < current_tail)  // have we possibly read
                                         // inconsistant stuff?
    {
        uint32_t lost = current_tail - list->next_read;
        list->lost += lost;              // update counter
        list->next_read = current_tail;  // try oldest avail. instead
        goto retry;
    }

    return 0;
}

void ferret_alist_init_consumer(void ** addr)
{
    ferret_alist_moni_t * temp;

    temp = malloc(sizeof(ferret_alist_moni_t));

    // setup some pointers
    temp->glob = *addr;  // store pointer to global structure
    temp->data = temp->glob->data;

    // copy header
    temp->header.major    = temp->glob->header.major;
    temp->header.minor    = temp->glob->header.minor;
    temp->header.instance = temp->glob->header.instance;
    temp->header.type     = temp->glob->header.type;

    // init. local information
    temp->lost      = 0;
    temp->next_read = 0;

    *addr = temp;
}

void ferret_alist_free_consumer(void ** addr)
{
    void * temp = ((ferret_alist_moni_t *)(*addr))->glob;

    free(*addr);

    *addr = temp;
}
