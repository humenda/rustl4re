/**
 * \file   ferret/lib/sensor/list_producer.c
 * \brief  List producer functions.
 *
 * \date   25/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
//#include <stdlib.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/list_producer.h>

void ferret_list_init_producer(void ** addr, void *(*alloc)(size_t size))
{
    ferret_list_local_t * temp;

    temp = (*alloc)(sizeof(ferret_list_local_t));

    // setup some pointers
    temp->glob    = *addr;  // store pointer to global structure
    temp->ind_buf = (ferret_list_index_t *)(temp->glob->data);
	temp->out_buf = temp->glob->data + temp->glob->element_offset;

    // copy header
    temp->header.major    = temp->glob->header.major;
    temp->header.minor    = temp->glob->header.minor;
    temp->header.instance = temp->glob->header.instance;
    temp->header.type     = temp->glob->header.type;

    *addr = temp;
}

void ferret_list_free_producer(void ** addr, void (*free)(void *))
{
    void * temp = ((ferret_list_local_t *)(*addr))->glob;

    (*free)(*addr);

    *addr = temp;
}
