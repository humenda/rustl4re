/**
 * \file   ferret/server/coord/sensors.h
 * \brief  Sensor management
 *
 * \date   04/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_SERVER_SENSOR_DIRECTORY_SENSORS_H_
#define __FERRET_SERVER_SENSOR_DIRECTORY_SENSORS_H_

#include <l4/dm_generic/types.h>
#include <l4/ferret/types.h>
#include <l4/l4vfs/tree_helper.h>
#include "ferret_monitor-server.h"

#define FERRET_MAX_SENSORS 100

typedef struct
{
    l4dm_dataspace_t  ds;
    int               major;
    int               minor;
    int               instance;
    int               type;
    int               flags;
    char            * config;
    l4vfs_th_node_t * node;  // pointer to corresponding tree_helper node
    char            * data;  // pointer to further, sensor-specific data
} sensor_entry_t;

/* Obviously a linear table is not the fastest data structure for
 * searching in it, feel free to optimize here if you have performance
 * problems!
 */
sensor_entry_t * sensors_new_entry(const l4dm_dataspace_t *ds,
                                   int major, int minor, int instance,
                                   int type, int flags, const char * config,
                                   void * data);
int sensors_get_index_for_id(int id);
int sensors_get_index_for_prop(int major, int minor, int instance);
void sensors_check_and_free(l4vfs_th_node_t * node);
int sensors_fill_list(ferret_monitor_list_entry_t * list, int offset,
                      int count);
//l4vfs_th_node_t * sensors_first_leaf(l4vfs_th_node_t * root);
l4vfs_th_node_t * sensors_next_nth_leaf(int n, l4vfs_th_node_t * start);

// debug stuff
void sensors_dump_tree(l4vfs_th_node_t * start, int indent);

#endif
