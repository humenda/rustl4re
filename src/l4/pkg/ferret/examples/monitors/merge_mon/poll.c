/**
 * \file   ferret/examples/merge_mon/poll.c
 * \brief  polls for outstanding sensors
 *
 * \date   2007-11-27
 * \author Ronald Aigner  <ra3@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>

#include <l4/sys/cache.h>
#include <l4/util/util.h>

#include <l4/ferret/monitor.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/list_consumer.h>
//#include <l4/ferret/sensors/tbuf_consumer.h>

#include "poll.h"
#include "main.h"

static void polling_thread(void* arg)
{
    int searching = 1;
    int i, ret;
    uint16_t type;

    while (searching)
    {
        l4_sleep(1000);
        searching = 0;

        for (i = 0; i < sensor_count; i++)
        {
            if (sensors[i].open)
                continue;

			l4_cap_idx_t srv = lookup_sensordir();

            searching = 1;
            ret = ferret_att(srv, sensors[i].major, sensors[i].minor,
                             sensors[i].instance, sensors[i].sensor);
            if (ret)
            {
                if (verbose)
                {
                    printf("Still could not attach to %hu:%hu:%hu, retrying!\n",
                           sensors[i].major, sensors[i].minor,
                           sensors[i].instance);
                }
                continue;
            }

            // this looks a bit racy ...
            sensors[i].copied    = 0;
            sensors[i].last_lost = 0;
            sensors[i].last_ts   = 0;

            type = ((ferret_common_t *)(sensors[i].sensor))->type;
            if (type != FERRET_LIST && type != FERRET_TBUF)
            {
                printf("Found wrong sensor type (%d): %hu!\n", i, type);
            }

            if (verbose)
            {
                printf("Attached to %hu:%hu:%hu.\n",
                       sensors[i].major, sensors[i].minor,
                       sensors[i].instance);
            }

            __asm__ __volatile__("": : :"memory");  // barrier
            sensors[i].open = 1;
        }
    }
}

void poll_sensors(void)
{
    l4thread_create_named(polling_thread, ".poll", 0, L4THREAD_CREATE_ASYNC);
}
