/**
 * \file   ferret/lib/client/create_dir.c
 * \brief  Function to create a new sensor at the directory for non
 *         L4Env clients.
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

#include <l4/dm_phys/dm_phys.h>
#include <l4/log/l4log.h>
#include <l4/sys/types.h>
#include <l4/util/macros.h>

#include <l4/ferret/ferret_client-client.h>

#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>

#include <l4/ferret/client.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/ulist_producer.h>

int ferret_create_sensor_dir(uint16_t major, uint16_t minor,
                             uint16_t instance, uint16_t type,
                             uint32_t flags, const char * config,
                             void ** addr, void *(*alloc)(size_t s),
                             l4_threadid_t dir)
{
    // explicitly set ferret_sensor_dir
    ferret_sensor_dir = dir;

    int ret;
    size_t size;
    l4dm_dataspace_t ds;
    CORBA_Environment _dice_corba_env = dice_default_environment;

    // create the sensor at the directory
    ret = ferret_client_create_call(&ferret_sensor_dir, major, minor,
                                    instance, type, flags, config, &ds,
                                    &_dice_corba_env);
    if (ret < 0)
    {
        LOG_Error("Creating sensor: %d", ret);
        return ret;
    }

    // now use the received dataspace ID to acquire the size
    ret = l4dm_mem_size(&ds, &size);
    if (ret)
    {
        LOG("Could not get size for dataspace: %u at "l4util_idfmt" !",
            ds.id, l4util_idstr(ds.manager));
        return ret;
    }

    // Check the *addr for == NULL, otherwise use l4rm_attach_to_region()
    if (*addr == NULL)
    {
        LOG("Could not attach dataspace, you must define destination address!");
        return -1;
    }
    else
    {
        ret = l4dm_map_ds(&ds, 0, (l4_addr_t)*addr, size, L4DM_RW);
        if (ret)
        {
            LOG("Could not attach dataspace to region %p!", *addr);
            return ret;
        }
    }

    switch(type)
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
        ferret_ulist_init_producer(addr, alloc);
        break;
    case FERRET_LIST:
        ferret_list_init_producer(addr, alloc);
        break;
    default:
        LOG("Unknown sensor type mapped: %d, ignored", type);
        return -1;
    }

    // fixme: check if we really need this all the time
    l4sigma0_kip_map(L4_INVALID_ID);
    ferret_calibrate_clock(FERRET_TIME_REL_US_FAST);

    return 0;
}
