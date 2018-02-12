/**
 * \file   ferret/server/coord/clients.c
 * \brief  Sensors management
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <l4/dm_generic/dm_generic.h>
#include <l4/log/l4log.h>

#include "sensors.h"
#include "dir.h"

#define MAX_PATH 100


sensor_entry_t * sensors_new_entry(const l4dm_dataspace_t *ds,
                                   int major, int minor, int instance,
                                   int type, int flags, const char * config,
                                   void * data)
{
    char path[MAX_PATH];
    l4vfs_th_node_t * parent_node;
    local_object_id_t parent_id;
    sensor_entry_t * se;
    l4vfs_th_node_t * node;

    // major
    snprintf(path, MAX_PATH, "/%d", major);
    parent_id = l4vfs_th_resolve(0, path);
    if (parent_id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        snprintf(path, MAX_PATH, "%d", major);
        parent_node = l4vfs_th_new_node(path, L4VFS_TH_TYPE_DIRECTORY,
                                        root, NULL);
        if (parent_node == NULL)
        {
            LOGd(verbose, "Failed to allocate new tree node, probably "
                 "out of memory?");
            return NULL;
        }
    }
    else
        parent_node = l4vfs_th_node_for_id(parent_id);

    // minor
    snprintf(path, MAX_PATH, "/%d/%d", major, minor);
    parent_id = l4vfs_th_resolve(0, path);
    if (parent_id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        snprintf(path, MAX_PATH, "%d", minor);
        parent_node = l4vfs_th_new_node(path, L4VFS_TH_TYPE_DIRECTORY,
                                        parent_node, NULL);
        if (parent_node == NULL)
        {
            LOGd(verbose, "Failed to allocate new tree node, probably "
                 "out of memory?");
            return NULL;
        }
    }
    else
        parent_node = l4vfs_th_node_for_id(parent_id);

    // instance
    snprintf(path, MAX_PATH, "%d", instance);
    se = malloc(sizeof(sensor_entry_t));
    if (se == NULL)
    {
        LOG("Failed to allocate new sensor_entry, probably out of memory?");
        return NULL;
    }
    node = l4vfs_th_new_node(path, L4VFS_TH_TYPE_OBJECT, parent_node, se);
    if (node == NULL)
    {
        LOG("Failed to allocate new tree node, probably out of memory?");
        free(se);
        return NULL;
    }

    se->ds          = *ds;
    se->node        = node;
    se->node->usage_count = 1;
    se->major       = major;
    se->minor       = minor;
    se->instance    = instance;
    se->type        = type;
    se->flags       = flags;
    se->config      = strdup(config);
    se->data        = data;

    return se;
}

#if 0
int sensors_get_index_for_id(int id)
{
    int i;
    for (i = 0; i < FERRET_MAX_SENSORS; i++)
    {
        if (sensor_table[i].usage_count > 0)
        {
            if (sensor_table[i].id == id)
                return i;
        }
    }
    return -1;
}

int sensors_get_index_for_prop(int major, int minor, int instance)
{
    int i;
    for (i = 0; i < FERRET_MAX_SENSORS; i++)
    {
        if (sensor_table[i].usage_count > 0)
        {
            if (sensor_table[i].major == major &&
                sensor_table[i].minor == minor &&
                sensor_table[i].instance == instance)
                return i;
        }
    }
    return -1;
}
#endif

void sensors_check_and_free(l4vfs_th_node_t * node)
{
    /* 1. check usage count
     * 2. == 0 && ! permanent flag -> close ds & cleanup node
     */

    int ret;
    sensor_entry_t * se = (sensor_entry_t *)(node->data);

    if (node->usage_count <= 0 && ! (se->flags & FERRET_PERMANENT))
    {
        ret = l4dm_close(&(se->ds));
        if (ret)
        {
            LOG("Problem closing dataspace: ret = %d!", ret);
        }

        free(se->config);
        free(se);
        ret = l4vfs_th_destroy_child(node->parent, node);
        if (ret)
        {
            LOG("Problem freeing node: ret = %d, ignored!", ret);
        }
    }
}

int sensors_fill_list(ferret_monitor_list_entry_t * list, int offset,
                      int count)
{
    /* 1. Find offset'th entry
     * 2. start filling from there the next count entries
     */

    int i;
    l4vfs_th_node_t * current;
    sensor_entry_t * se;

    for (i = 0, current = sensors_next_nth_leaf(offset, root);
         i < count && current != NULL;
         i++, current = sensors_next_nth_leaf(1, current))
    {
        se = (sensor_entry_t *)current->data;
        list[i].major    = se->major;
        list[i].minor    = se->minor;
        list[i].instance = se->instance;
        list[i].type     = se->type;
        list[i].id       = current->id;
    }
    return i;
}

#if 0
l4vfs_th_node_t * sensors_first_leaf(l4vfs_th_node_t * root)
{
    return sensors_next_nth_leaf(0, root)
}

l4vfs_th_node_t * sensors_first_leaf(l4vfs_th_node_t * root)
{
    l4vfs_th_node_t * ret;
    l4vfs_th_node_t * current = root;

    for (; current != NULL; current = current->next)
    {
        if (current->type == L4VFS_TH_TYPE_OBJECT)
        {
            return current;
        }
        else
        {
            ret = sensors_first_leaf(current);
            if (ret != NULL)
                return ret;
        }
    }

    return NULL;
}
#endif

/**
 * @brief Traverse the tree, starting at start to find the n'th leaf
 *        node.
 *
 * If start is a leaf node itself, give n == 1 to find the next leaf
 * according to depth-first-search.  If start is a non-leaf node
 * specify n == 0 to find the first leaf.
 *
 * @param n find the n'th leaf node after start
 * @param start node to start lookup at in the tree
 *
 * @return pointer to found node or NULL if nothing was found
 */
l4vfs_th_node_t * sensors_next_nth_leaf(int n, l4vfs_th_node_t * start)
{
    typedef enum direction_e {DOWN, NEXT, UP} direction_t;
    direction_t dir = DOWN;
    l4vfs_th_node_t * current = start;

    /* 1. check for object type and n < 0 -> yes? return current
     * 2. switch direction
     *     - DOWN: go down if possible (current = current->first_child)
     *       -> yes? continue
     *          no? switch to NEXT
     *     - NEXT: go next if possible (current = current->next)
     *       -> yes? dir = DOWN; continue
     *          no? switch to UP
     *     - UP: go up if possible (current = current->parent)
     *       -> yes? dir = NEXT; continue
     *          no? return NULL
     */

    while (1)
    {
        if (current->type == L4VFS_TH_TYPE_OBJECT)
        {
            n--;
            if (n < 0)
                return current;
        }

        switch (dir)
        {
        case DOWN:
            if (current->first_child)
            {
                current = current->first_child;
                continue;
            }
            else
            {
                dir = NEXT;
            }
            // fall through
        case NEXT:
            if (current->next)
            {
                current = current->next;
                dir = DOWN;
                continue;
            }
            else
            {
                dir = UP;
            }
            // fall through
        case UP:
            if (current->parent)
            {
                current = current->parent;
                dir = NEXT;
                continue;
            }
            else
            {
                return NULL;
            }
        }
    }
}


void sensors_dump_tree(l4vfs_th_node_t * start, int indent)
{
    l4vfs_th_node_t * current;
    sensor_entry_t * se;
    int i;

    for (current = start; current; current = current->next)
    {
        se = (sensor_entry_t *)(current->data);
        for (i = 0; i < indent; i++)
            printf("    ");
        if (se)
        {
            printf("%d:%d:%d (%d)\n",
                   se->major, se->minor, se->instance, current->id);
        }
        else
        {
            printf("%s (%d)\n", current->name, current->id);
        }
        sensors_dump_tree(current->first_child, indent + 1);
    }
}
