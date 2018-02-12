/**
 * \file   ferret/lib/sensor/l4lx_client.c
 * \brief  Helper function for L4Linux user mode clients.
 *
 * \date   2006-04-03
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/ferret/types.h>
#include <l4/ferret/l4lx_client.h>

static ferret_list_local_t _list;
ferret_list_local_t * ferret_l4lx_user = NULL;

static void * _alloc(size_t s)
{
    return &_list;
}

void ferret_list_l4lx_user_init(void)
{
    ferret_l4lx_user = (ferret_list_local_t *)(0x8000 + 0xBFC00000UL + 0x1000);
    ferret_list_init_producer((void **)&ferret_l4lx_user, &_alloc);
}
