/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_SERVER_SENSOR_DIRECTORY_CLIENTS_H_
#define __FERRET_SERVER_SENSOR_DIRECTORY_CLIENTS_H_

#include <l4/dm_generic/types.h>
#include <l4/l4vfs/tree_helper.h>
#include <l4/l4vfs/types.h>

#define MAX_CLIENTS 1024

typedef struct client_s
{
    int               open;
    int               mode;
    l4vfs_size_t      seek;
    l4_threadid_t     client;
    l4vfs_th_node_t * node;
} client_t;

extern client_t clients[MAX_CLIENTS];

int  clients_get_free(void);
int  clients_is_open(int handle);

#endif
