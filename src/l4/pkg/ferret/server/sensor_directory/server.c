/**
 * \file   ferret/server/sensor_directory/server.c
 * \brief  Implements IDL function templates
 *
 * \date   07/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <l4/dm_generic/dm_generic.h>
#include <l4/dm_phys/dm_phys.h>
#include <l4/log/l4log.h>

#include "ferret_dir-server.h"

#include "dir.h"
#include "sensors.h"
#include "clients.h"

#include <l4/ferret/sensors/scalar_init.h>
#include <l4/ferret/sensors/histogram_init.h>
#include <l4/ferret/sensors/list_init.h>
#include <l4/ferret/sensors/alist_init.h>
#include <l4/ferret/sensors/dplist_init.h>
#include <l4/ferret/sensors/slist_init.h>
#include <l4/ferret/sensors/ulist_init.h>
#include <l4/ferret/sensors/tbuf.h>

#include <l4/l4vfs/types.h>
#include <l4/l4vfs/name_space_provider.h>


#define MAX_PATH 100

static int instance_counter = 1;  // 0 reserved for ROOT instance


int32_t
ferret_client_create_component(CORBA_Object _dice_corba_obj,
                               uint16_t major,
                               uint16_t minor,
                               uint16_t instance,
                               uint16_t type,
                               uint32_t flags,
                               const char* config,
                               l4dm_dataspace_t *ds,
                               CORBA_Server_Environment *_dice_corba_env)
{
    int ret;
    void * temp;
    char * data;
    ssize_t size = 0;
    char path[MAX_PATH];
    l4vfs_th_node_t * node;
    local_object_id_t id;
    sensor_entry_t * se;

    LOGd(verbose, "create: %hd:%hd:%hd:%hd", major, minor, instance, type);
    // fixme: - check flags
    //        - care for shared opens
    //        - care for usage count on reopens

    /* 1. check of we have this node -> reopen
     *    yes ? -> as usual + increment usage count
     *    no ?  -> create directories on demand and finally create the
     *             corresponding node itself
     */

    snprintf(path, MAX_PATH, "/%d/%d/%d", major, minor, instance);
    id = l4vfs_th_resolve(0, path);
    if (id != L4VFS_ILLEGAL_OBJECT_ID)
    {
        node = l4vfs_th_node_for_id(id);
        se = (sensor_entry_t *)(node->data);
        // check some properties, reopen works only with similar properties
        if (type != se->type)
        {
            LOG("Given type '%d' does not match old one '%d' on reopen!",
                type, se->type);
            return -1;
        }
        if (flags != se->flags)
        {
            LOG("Given flags '%d' do not match old ones '%d' on reopen!",
                type, se->flags);
            return -1;
        }
        if (strcmp(config, se->config) != 0)
        {
            LOG("Given config '%s' does not match old one '%s' on reopen!",
                config, se->config);
            return -1;
        }

        *ds = se->ds;
        ret = l4dm_share(ds, *_dice_corba_obj, L4DM_RW);
        if (ret)
        {
            LOG("Cannot share access rights for ds: ret = %d", ret);
            return -1;
        }
        node->usage_count++;
    }
    else
    {
        unsigned dm_flags = L4DM_PINNED + L4RM_MAP + L4DM_CONTIGUOUS;

        // create ds
        switch (type)
        {
        case FERRET_SCALAR:
            size = ferret_scalar_size_config(config);
            break;
        case FERRET_HISTO:
            size = ferret_histo_size_config(config);
            break;
        case FERRET_HISTO64:
            size = ferret_histo64_size_config(config);
            break;
        case FERRET_LIST:
            size = ferret_list_size_config(config);
            break;
        case FERRET_DPLIST:
            size = ferret_dplist_size_config(config);
            break;
        case FERRET_SLIST:
            size = ferret_slist_size_config(config);
            break;
        case FERRET_ULIST:
            size = ferret_ulist_size_config(config);
            break;
        case FERRET_ALIST:
            size = ferret_alist_size_config(config);
            break;
        case FERRET_TBUF:
            LOG("The tracebuffer sensor can not be instantiated, it exists"
                " in the kernel!");
            return -1;
        default:
            LOG("Unknown sensor type requested: %d", type);
            return -1;
        }
        LOGd(verbose, "Size: %d", (int)size);  // %z in not printf compatible
        if (flags & FERRET_SUPERPAGES && size > L4_PAGESIZE)
            dm_flags += L4DM_MEMPHYS_SUPERPAGES;
        temp = l4dm_mem_ds_allocate(size, dm_flags, ds);
        if (! temp)
            return -1;

        ferret_common_init((ferret_common_t *)temp, major, minor, instance,
                           type, *ds);
        switch (type)
        {
        case FERRET_SCALAR:
            ret = ferret_scalar_init((ferret_scalar_t *) temp, config);
            if (ret != 0)
            {
                LOG("Something wrong in scalar_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_HISTO:
            ret = ferret_histo_init((ferret_histo_t *) temp, config);
            if (ret != 0)
            {
                LOG("Something wrong in histo_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_HISTO64:
            ret = ferret_histo64_init((ferret_histo64_t *) temp, config);
            if (ret != 0)
            {
                LOG("Something wrong in histo64_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_LIST:
            ret = ferret_list_init((ferret_list_t *) temp, config);
            if (ret != 0)
            {
                LOG("Something wrong in list_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_ALIST:
            ret = ferret_alist_init((ferret_alist_t *) temp, config);
            if (ret != 0)
            {
                LOG("Something wrong in alist_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_DPLIST:
            ret = ferret_dplist_init((ferret_dplist_t *) temp, config, &data);
            if (ret != 0)
            {
                LOG("Something wrong in dplist_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_SLIST:
            ret = ferret_slist_init((ferret_slist_t *) temp, config, &data);
            if (ret != 0)
            {
                LOG("Something wrong in slist_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        case FERRET_ULIST:
            ret = ferret_ulist_init((ferret_ulist_t *) temp, config, &data);
            if (ret != 0)
            {
                LOG("Something wrong in ulist_init: %d, %s", ret, config);
                // fixme: free allocated memory
                return -2;
            }
            break;
        default:
            LOG("Unknown sensor type requested: %d", type);
            return -1;
        }
        LOGd(verbose, "Size: %d", (int)size);  // %z in not printf compatible

        se = sensors_new_entry(ds, major, minor, instance, type, flags,
                               config, data);
        if (! se)
        {
            return -1;
        }
        id = se->node->id;
        LOGd(verbose, "Registered with id = %d.", id);

        ret = l4dm_share(ds, *_dice_corba_obj, L4DM_RW);
        if (ret)
        {
            // free ds
            l4dm_mem_release(temp);

            LOG("Cannot share access rights for ds: ret = %d", ret);
            return -1;
        }

    }

    return 0;
}


int32_t
ferret_client_free_component(CORBA_Object _dice_corba_obj,
                             uint16_t major,
                             uint16_t minor,
                             uint16_t instance,
                             CORBA_Server_Environment *_dice_corba_env)
{
    #warning "ferret_client_free_sensor_component is not implemented!"
    return -1;
}


int32_t
ferret_client_new_instance_component(CORBA_Object _dice_corba_obj,
                                     CORBA_Server_Environment *_dice_corba_env)
{
    // fixme: care for overflow
    return instance_counter++;
}


int32_t
ferret_monitor_attach_component(CORBA_Object _dice_corba_obj,
                                uint16_t major,
                                uint16_t minor,
                                uint16_t instance,
                                l4dm_dataspace_t *ds,
                                CORBA_Server_Environment *_dice_corba_env)
{
    local_object_id_t id;
    l4vfs_th_node_t * node;
    char path[MAX_PATH];
    int ret;

    if (major == FERRET_TBUF_MAJOR && minor == FERRET_TBUF_MINOR)
    {
        LOGd(verbose, "Request for trace buffer");
        return -2;  // not found
    }

    snprintf(path, MAX_PATH, "/%d/%d/%d", major, minor, instance);
    id = l4vfs_th_resolve(0, path);
    if (id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        LOGd(verbose, "Request for unknown sensor");
        return -1;  // not found
    }
    node = l4vfs_th_node_for_id(id);
    *ds = ((sensor_entry_t *)(node->data))->ds;

    LOGd(verbose, "Found id = %d.", id);
    ret = l4dm_share(ds, *_dice_corba_obj, L4DM_RO);
    if (ret)
    {
        LOG("Cannot share access rights for ds: ret = %d", ret);
        return -1;
    }

    node->usage_count++;

    return 0;
}


int32_t
ferret_monitor_detach_component(CORBA_Object _dice_corba_obj,
                                uint16_t major,
                                uint16_t minor,
                                uint16_t instance,
                                CORBA_Server_Environment *_dice_corba_env)
{
    local_object_id_t id;
    l4vfs_th_node_t * node;
    int ret;
    char path[MAX_PATH];
    l4dm_dataspace_t ds;

    snprintf(path, MAX_PATH, "/%d/%d/%d", major, minor, instance);
    id = l4vfs_th_resolve(0, path);
    if (id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        LOGd(verbose, "Request for unknown sensor");
        return -1;  // not found
    }
    node = l4vfs_th_node_for_id(id);
    ds = ((sensor_entry_t *)(node->data))->ds;

    ret = l4dm_revoke(&ds, *_dice_corba_obj, L4DM_ALL_RIGHTS);
    if (ret)
    {
        LOG("Cannot revoke access rights for ds: ret = %d", ret);
        return -1;
    }
    node->usage_count--;
    sensors_check_and_free(node);

    return 0;
}


int32_t
ferret_monitor_list_component(CORBA_Object _dice_corba_obj,
                              ferret_monitor_list_entry_t **entries,
                              int32_t *count,
                              int32_t offset,
                              CORBA_Server_Environment *_dice_corba_env)
{
    LOGd(verbose, "list request with offset = '%d', count = '%d'.",
         offset, *count);
    //sensors_dump_tree(root, 0);

    *count = sensors_fill_list(*entries, offset, *count);

    if (*count < 0)
        return -1;
    else
        return 0;
}


/****************************** L4VFS stuff ******************************/


l4_threadid_t
l4vfs_basic_name_server_thread_for_volume_component(
    CORBA_Object _dice_corba_obj,
    volume_id_t volume_id,
    CORBA_Server_Environment *_dice_corba_env)
{
    if (volume_id == FERRET_L4VFS_VOLUME_ID)
    {
        return l4_myself();
    }
    return L4_INVALID_ID;
}


object_handle_t
l4vfs_basic_io_open_component(CORBA_Object _dice_corba_obj,
                              const object_id_t *object_id,
                              int32_t flags,
                              CORBA_Server_Environment *_dice_corba_env)
{
    /* 1. check if valid id
     * 2. check of mode == RO
     * 3. check if dir
     * 4. create a new client state
     * 5. return handle
     */
    l4vfs_th_node_t * node;
    int handle;

    // 1.
    if (object_id->volume_id != FERRET_L4VFS_VOLUME_ID)
        return -ENOENT;
    node = l4vfs_th_node_for_id(object_id->object_id);
    if (node == NULL)
        return -ENOENT;

    // 2.
    if ((flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;

    // 3.
    if (node->type != L4VFS_TH_TYPE_DIRECTORY)
        return -EINVAL;

    // 4.
    handle = clients_get_free();
    if (handle < 0)
        return ENOMEM;
    clients[handle].open   = 1;
    clients[handle].mode   = flags;
    clients[handle].seek   = 0;
    clients[handle].client = *_dice_corba_obj;
    clients[handle].node   = node;
    node->usage_count++;

    return handle;
}


int32_t
l4vfs_common_io_close_component(CORBA_Object _dice_corba_obj,
                                object_handle_t handle,
                                CORBA_Server_Environment *_dice_corba_env)
{
    if (handle < 0 || handle >= MAX_CLIENTS)
        return -EBADF;
    if (! clients_is_open(handle))
        return -EBADF;
    if (! l4_task_equal(clients[handle].client, *_dice_corba_obj))
        return -EBADF;
    if (clients[handle].node->type != L4VFS_TH_TYPE_DIRECTORY)
        return -EBADF;

    clients[handle].node->usage_count--;
    clients[handle].open = 0;

    // fixme: check for cleaning up dirs here later
    if (clients[handle].node->usage_count <= 0)
    {
        //_check_and_clean_node(clients[handle].node);
    }

    return 0;
}


object_id_t
l4vfs_basic_name_server_resolve_component(
    CORBA_Object _dice_corba_obj,
    const object_id_t *base,
    const char* pathname,
    CORBA_Server_Environment *_dice_corba_env)
{
    local_object_id_t l_id;
    object_id_t id;

    LOG("resolve called with: %d:%d, '%s'",
        base->volume_id, base->object_id, pathname);

    if (base->volume_id != FERRET_L4VFS_VOLUME_ID)
    {
        id.volume_id = L4VFS_ILLEGAL_VOLUME_ID;
        id.object_id = L4VFS_ILLEGAL_OBJECT_ID;
        return id;
    }

    l_id = l4vfs_th_resolve(base->object_id, pathname);

    if (l_id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        id.volume_id = L4VFS_ILLEGAL_VOLUME_ID;
        id.object_id = L4VFS_ILLEGAL_OBJECT_ID;
    }
    else
    {
        id.volume_id = FERRET_L4VFS_VOLUME_ID;
        id.object_id = l_id;
    }
    return id;
}


char*
l4vfs_basic_name_server_rev_resolve_component(
    CORBA_Object _dice_corba_obj,
    const object_id_t *dest,
    object_id_t *parent,
    CORBA_Server_Environment *_dice_corba_env)
{
    char * ret;

    if (dest->volume_id != FERRET_L4VFS_VOLUME_ID ||
        parent->volume_id != FERRET_L4VFS_VOLUME_ID)
    {
        return NULL;
    }

    ret = l4vfs_th_rev_resolve(dest->object_id, &(parent->object_id));

    // tell dice to free the pointer after the reply
    dice_set_ptr(_dice_corba_env, ret);

    return ret;
}


int32_t
l4vfs_basic_io_getdents_component(CORBA_Object _dice_corba_obj,
                                  object_handle_t handle,
                                  l4vfs_dirent_t **dirp,
                                  uint32_t *count,
                                  CORBA_Server_Environment *_dice_corba_env)
{
    int ret, seek;
    l4vfs_th_node_t * node;
    int __count;

    if (*count > INT_MAX)
        __count = INT_MAX;
    else
        __count = (int)*count;

    if (handle < 0 || handle >= MAX_CLIENTS)
        return -EBADF;
    if (! clients_is_open(handle))
        return -EBADF;
    if (! l4_task_equal(clients[handle].client, *_dice_corba_obj))
        return -EBADF;

    node = clients[handle].node;
    seek = clients[handle].seek;
    ret = l4vfs_th_dir_fill_dirents(node, seek, *dirp, &__count);
    clients[handle].seek = ret;  // set new seekpointer

    if (__count < 0)
        return -EINVAL;
    else
        return *count;
}


int32_t
l4vfs_basic_io_stat_component(CORBA_Object _dice_corba_obj,
                              const object_id_t *object_id,
                              l4vfs_stat_t *buf,
                              CORBA_Server_Environment *_dice_corba_env)
{
    /* 1. check if object exists
     * 2. fill data structure
     * 3. return it
     */
    l4vfs_th_node_t * node;

    // 1.
    if (object_id->volume_id != FERRET_L4VFS_VOLUME_ID)
        return -ENOENT;
    node = l4vfs_th_node_for_id(object_id->object_id);
    if (node == NULL)
        return -ENOENT;

    // 2.
    buf->st_dev   = FERRET_L4VFS_VOLUME_ID;
    buf->st_ino   = object_id->object_id;
    if (node->type == L4VFS_TH_TYPE_OBJECT)
        buf->st_mode = S_IFREG;
    else
        buf->st_mode  = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    buf->st_nlink = 1;
    // fixme: other fields are undefined for now ...

    return 0;
}


l4vfs_off_t
l4vfs_basic_io_lseek_component(CORBA_Object _dice_corba_obj,
                               object_handle_t handle,
                               l4vfs_off_t offset,
                               int32_t whence,
                               CORBA_Server_Environment *_dice_corba_env)
{
    if (handle < 0 || handle >= MAX_CLIENTS)
        return -EBADF;
    if (! clients_is_open(handle))
        return -EBADF;
    if (! l4_task_equal(clients[handle].client, *_dice_corba_obj))
        return -EBADF;

    switch (whence)  // fixme: care for all the other cases ...
    {
    case SEEK_SET:
        if (offset != 0)
            return -EINVAL;
        clients[handle].seek = offset;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}


l4vfs_ssize_t
l4vfs_common_io_read_component(CORBA_Object _dice_corba_obj,
                               object_handle_t fd,
                               char **buf,
                               l4vfs_size_t *count,
                               short *_dice_reply,
                               CORBA_Server_Environment *_dice_corba_env)
{
    // fixme: we could distinguish between EINVAL and EBADF
    //        etc. depending on whether we have this file open or not
    return -EINVAL;
}


l4vfs_ssize_t
l4vfs_common_io_write_component(CORBA_Object _dice_corba_obj,
                                object_handle_t fd,
                                const char *buf,
                                l4vfs_size_t *count,
                                short *_dice_reply,
                                CORBA_Server_Environment *_dice_corba_env)
{
    // fixme: we could distinguish between EINVAL and EBADF
    //        etc. depending on whether we have this file open or not
    return -EINVAL;
}

int32_t
ferret_fpages_request_component (CORBA_Object _dice_corba_obj,
                                 uint16_t major,
                                 uint16_t minor,
                                 uint16_t instance,
                                 l4_snd_fpage_t *page,
                                 CORBA_Server_Environment *_dice_corba_env)
{
    local_object_id_t id;
    l4vfs_th_node_t * node;
    char path[MAX_PATH];
    sensor_entry_t * se;
    int ksem;

    LOG("entry");

    snprintf(path, MAX_PATH, "/%d/%d/%d", major, minor, instance);
    id = l4vfs_th_resolve(0, path);
    if (id == L4VFS_ILLEGAL_OBJECT_ID)
    {
        LOGd(verbose, "Request for unknown sensor");
        return -1;  // not found
    }
    node = l4vfs_th_node_for_id(id);
    se = (sensor_entry_t *)(node->data);
    if (se->type != FERRET_ULIST)
    {
        LOGd(verbose, "Request on wrong sensor type for this operation");
        return -2;
    }
    ksem = (int)se->data;

    page->snd_base = 0;  // fixme: is this right?
    page->fpage.iofp.grant  = 0;
    page->fpage.iofp.zero1  = 0;
    page->fpage.iofp.iosize = 0;
    page->fpage.iofp.zero2  = 2;
    page->fpage.iofp.iopage = ksem;
    page->fpage.iofp.f      = 0xf;

    LOG("exit");

    return 0;
}
