/**
 * \file   ferret/lib/client/instance.c
 * \brief  Function to get a new instance at sensors directory.
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

#include <l4/ferret/ferret_client-client.h>

#include <l4/ferret/client.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/list_producer.h>

int ferret_create_instance()
{
    CORBA_Environment _dice_corba_env = dice_default_environment;
    _dice_corba_env.malloc = (dice_malloc_func)malloc;
    _dice_corba_env.free = (dice_free_func)free;

    // get ferret_sensor_dir task id lazily
    if (l4_is_invalid_id(ferret_sensor_dir))
        ferret_sensor_dir = ferret_comm_get_threadid();

    return ferret_client_new_instance_call(&ferret_sensor_dir,
                                           &_dice_corba_env);
}
