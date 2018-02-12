/**
 * \file   ferret/lib/client/free.c
 * \brief  Function to release sensors at directory.
 *
 * \date   2006-04-06
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

#include <l4/l4rm/l4rm.h>
#include <l4/log/l4log.h>

#include <l4/ferret/ferret_client-client.h>
#include <l4/ferret/types.h>
#include <l4/ferret/client.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/ulist_producer.h>

// fixme: we don't really need the first three attributes, as these
//        are available in the sensor's head
int ferret_free_sensor(uint16_t major, uint16_t minor,
                       uint16_t instance, void * addr,
                       void (*free)(void *))
{
    int ret;
    CORBA_Environment _dice_corba_env = dice_default_environment;
    _dice_corba_env.malloc = (dice_malloc_func)malloc;
    _dice_corba_env.free = (dice_free_func)free;

    // get ferret_sensor_dir task id lazily
    if (l4_is_invalid_id(ferret_sensor_dir))
        ferret_sensor_dir = ferret_comm_get_threadid();

    switch(((ferret_common_t *)addr)->type)
    {
    case FERRET_SCALAR:
    case FERRET_HISTO:
    case FERRET_HISTO64:
    case FERRET_DPLIST:
    case FERRET_SLIST:
    case FERRET_ALIST:
        // no special setup required
        break;
    case FERRET_ULIST:
        ferret_ulist_free_producer(&addr, free);
        break;
    case FERRET_LIST:
        ferret_list_free_producer(&addr, free);
        break;
    default:
        LOG("Unknown sensor type mapped: %d, ignored",
            ((ferret_common_t *)addr)->type);
        return -1;
    }

    // unmap shared memory
    ret = l4rm_detach(addr);
    if (ret)
    {
        LOG("Could not dettach dataspace, ignored!");
    }

    return ferret_client_free_call(&ferret_sensor_dir, major, minor,
                                    instance, &_dice_corba_env);
}
