/**
 * \file   ferret/lib/sensor/__llist_producer.c
 * \brief  locked list producer functions.
 *
 * \date   2007-06-19
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

void PREFIX(init_producer)(void ** addr, void *(*alloc)(size_t size))
{
    PREFIX(local_t) * temp;

    temp = (*alloc)(sizeof(PREFIX(local_t)));

    // setup some pointers
    temp->glob    = *addr;  // store pointer to global structure

    // copy header
    temp->header.major    = temp->glob->header.major;
    temp->header.minor    = temp->glob->header.minor;
    temp->header.instance = temp->glob->header.instance;
    temp->header.type     = temp->glob->header.type;

#ifdef FERRET_LLIST_HAVE_LOCAL_LOCK
    FERRET_LLIST_LOCAL_INIT(temp);
#endif

    *addr = temp;
}

void PREFIX(free_producer)(void ** addr, void (*free)(void *))
{
    void * temp = ((PREFIX(local_t) *)(*addr))->glob;

    // fixme: remove local mappings
    (*free)(*addr);

    *addr = temp;
}
