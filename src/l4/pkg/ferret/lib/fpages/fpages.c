/**
 * \file   ferret/lib/fpages/fpages.c
 * \brief  IDL wrapper for fpages interface
 *
 * \date   2007-06-21
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006, 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

#include <l4/l4rm/l4rm.h>
#include <l4/log/l4log.h>

#include <l4/ferret/ferret_fpages-client.h>
#include <l4/ferret/types.h>
#include <l4/ferret/client.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/fpages.h>

int ferret_fpages_request(int ksem, uint16_t major, uint16_t minor,
                          uint16_t instance)
{
    int ret;
    l4_snd_fpage_t page;

    CORBA_Environment _dice_corba_env = dice_default_environment;
    _dice_corba_env.malloc = (dice_malloc_func)malloc;
    _dice_corba_env.free = (dice_free_func)free;

    // get ferret_sensor_dir task id lazily
    if (l4_is_invalid_id(ferret_sensor_dir))
        ferret_sensor_dir = ferret_comm_get_threadid();

    _dice_corba_env.rcv_fpage = l4_iofpage(ksem, 0, 0);
    _dice_corba_env.rcv_fpage.iofp.zero2 = 2;
    _dice_corba_env.rcv_fpage.raw       |= 2;  // what is this?

    ret = ferret_fpages_request_call(&ferret_sensor_dir, major, minor,
                                     instance, &page, &_dice_corba_env);
    return ret;
}
