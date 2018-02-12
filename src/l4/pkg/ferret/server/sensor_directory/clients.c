/**
 * \file   ferret/server/sensor_directory/clients.c
 * \brief  Client handling.
 *
 * \date   02/22/2006
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "clients.h"

client_t clients[MAX_CLIENTS];

int  clients_get_free(void)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].open == 0)
            return i;
    }
    return -1;
}

int  clients_is_open(int handle)
{
    return clients[handle].open;
}
