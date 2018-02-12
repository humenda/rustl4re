/**
 * \file   ferret/examples/merge_mon/buffer.c
 * \brief  Buffer management stuff.
 *
 * \date   13/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <alloca.h> // alloca
#include <stdlib.h> 
#include <stdint.h> 
#include <stdio.h>  // printf
#include <string.h> // strncpy

//#include <l4/lock/lock.h>
//#include <l4/ore/uip-ore.h>
#include <l4/util/util.h>
#include <l4/util/rdtsc.h>
//#include <l4/semaphore/semaphore.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/maj_min.h>
#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/list_consumer.h>
//#include <l4/ferret/sensors/tbuf_consumer.h>

#include "buffer.h"
#include "gui.h"
#include "main.h"
//#include "net.h"
#include "uu.h"

extern int dump_after;

/*
 * This function must be called with the buffer lock already held.
 */
void buf_set_start_ts(buf_t * buf)
{
    buf->start_ts = l4_rdtsc();  // open a new recording window
    buf->stop_ts  = 0;
}

/*
 * This function must be called with the buffer lock already held.
 */
void buf_set_stop_ts(buf_t * buf)
{
    buf->stop_ts = l4_rdtsc();   // close recording window
}

/*
 * This function must be called with the buffer lock already held.
 */
void buf_set_offline(buf_t * buf)
{
    buf->start_ts = 2;
    buf->stop_ts = 1;
}

/*
 * This function must be called with the buffer lock already held.
 */
void buf_set_online(buf_t * buf)
{
    buf->start_ts = 0;
    buf->stop_ts = 0;
}

/*
 * This function must be called with the buffer lock already held.
 */
void buf_set_cyclic(buf_t * buf)
{
    buf->cyclic = 1;
}

/*
 * This function must be called with the buffer lock already held.
 */
void buf_clear(buf_t * buf)
{
    int i;

    buf->write_pos = buf->start;
    buf->dump_pos  = buf->start;
    buf->full      = 0;
    for (i = 0; i < sensor_count; i++)
    {
        sensors[i].copied = 0;
    }
}


static void buf_dumpz(buf_t *buf, char const *filename)
{
    char name[48];
    strncpy(name, filename, 48);
    name[47] = 0;
    uu_dumpz(name, buf->start, buf->write_pos - buf->start);
}

/*
 * This function must be called with the buffer lock already held.
 */
static int buf_is_valid_ts(const buf_t * buf, ferret_utime_t ts)
{
    if ((buf->start_ts <= ts) && (buf->stop_ts > ts || buf->stop_ts == 0))
        return 1;  // no restriction
    else
        return 0;  // out of interval
}

/*
 * Check whether an event is a control event.
 *
 * This function must be called with the buffer lock already held.
 */
static void buf_check_control_events(buf_t * buf,
                                     ferret_list_entry_t * list_e_pu,
                                     unsigned int size)
{
    ferret_list_entry_common_t * e = (ferret_list_entry_common_t *)list_e_pu;

    if (size < sizeof(ferret_list_entry_common_t))
        return;

    if (e->major == FERRET_MONCON_MAJOR)
    {
        switch (e->minor)
        {
        case FERRET_MONCON_START:
            buf_set_start_ts(buf);
            break;
        case FERRET_MONCON_STOP:
            buf_set_stop_ts(buf);
            break;
#if 0
        case FERRET_MONCON_NETSEND:
            if (local_ip[0] != '\0')
            {
                int ret;
                struct in_addr ip;
                unsigned short port;

                ret = l4semaphore_try_down(&net_sem);
                if (ret == 0)
                {
                    printf("Network transfer already in progress!\n");
                    return;
                }

                port = e->data16[0];
                memcpy(&ip, &(e->data16[1]), sizeof(ip));
                printf("Destination address: '%s:%d'\n", inet_ntoa(ip) , port);
                ret = uip_ore_connect(ip, port);
                if (ret)
                {
                    printf("Error connecting: %d!\n", ret);
                    l4semaphore_up(&net_sem);
                    return;
                }
            }
            else
            {
                printf("No local IP address given, not doing anything!\n");
            }
            break;
#endif
        case FERRET_MONCON_SERSEND:
            buf_dumpz(buf, (char const*)e->data8);
            break;
        case FERRET_MONCON_CLEAR:
            buf_clear(buf);
            break;
        case FERRET_MONCON_STATS:
        {
            int i;
            printf("Stats:\n");
            printf("  Buffer (start, size, write_pos): %p, %zd, %p\n",
                   buf->start, buf->size, buf->write_pos);
            printf("  Times (start, stop): %llu, %llu\n",
                   buf->start_ts, buf->stop_ts);
            printf("Sensors:\n");
            for (i = 0; i < sensor_count; ++i)
            {
                if (sensors[i].open == 0)
                    continue;
                printf("  %s <%hu, %hu, %hu>\n", sensors[i].name,
                           sensors[i].major, sensors[i].minor,
                           sensors[i].instance);
            }
            break;
        }
        case FERRET_MONCON_PING:
            printf("Yes, I'm here up and running!\n");
            break;
        }
    }
}

/* retval: -1 -> continue
 * retval: -2 -> break out
 *
 * This function must be called with the buffer lock already held.
 */
static int
buf_copy_event(buf_t * buf, ferret_list_entry_t * ev, int size, int sensor)
{
    // continue if we already know the buffer is full
    if (buf->full)
        return -1;

    if (! buf_is_valid_ts(buf, ev->timestamp))
    {  // skip this event as it is out of our time range
        return -1;
    }

    // check if buffer memory is full
    if (buf->write_pos + size + sizeof(int) >=
        buf->start + buf->size)
    {
        if (buf->cyclic)
        {
            buf_clear(buf);
        }
        else
        {
            buf->full = 1;
            return -2;
        }
    }

    // we have some space left, so save event data
    *((int *)(buf->write_pos)) = size;
    buf->write_pos += sizeof(int);
    memcpy(buf->write_pos, ev, size);
    buf->write_pos += size;
    sensors[sensor].copied++;
    sensors[sensor].last_ts = ev->timestamp;

    return 0;
}


int buf_fill(buf_t * buf, int max_size, int interval, int verbose __attribute__((unused)))
{
    int i, ret;
    ferret_list_entry_kernel_t * list_e_pk;
    list_e_pk = alloca(max_size);
    // make an alias to same memory region
    ferret_list_entry_t * list_e_pu = (ferret_list_entry_t *)list_e_pk;
    ferret_list_entry_common_t loss_event;
    int count = 0;

    // copy elements
    while (1)
    {
        uint16_t type;
        int size = 0;

        pthread_mutex_lock(&buf->lock);

        if ((dump_after != 0) &&
            (count == dump_after)) {
            buf_dumpz(buf, "dump.log");
            count = 0;
        }
        count++;

        for (i = 0; i < sensor_count; ++i)
        {
            /* 1. test sensor type
             * 2. execute getter function according to type and store
             *    events, prefix-size coded into memory buffer
             * 3. stop if buffer full
             */
            if (sensors[i].open == 0)
                continue;

            type = ((ferret_common_t *)(sensors[i].sensor))->type;

            while (1)
            {
                if (type == FERRET_LIST)
                {
                    ret = ferret_list_get(
                        (ferret_list_moni_t *)(sensors[i].sensor), list_e_pu);
                    if (ret == -1)
                    {
                        break;
                    }
                    else if (ret < -1)
                    {
                        printf(
                            "Something wrong with sensor %s (%hu:%hu:%hu):"
                            " %d\n", sensors[i].name,
                            sensors[i].major, sensors[i].minor,
                            sensors[i].instance, ret);
                    }
                    size = ((ferret_list_moni_t *)
                            (sensors[i].sensor))->glob->element_size;
                    buf_check_control_events(buf, list_e_pu, size);

                    // check lost count here
                    if (((ferret_list_moni_t *)(sensors[i].sensor))->lost >
                        sensors[i].last_lost)
                    {
                        uint64_t count =
                            ((ferret_list_moni_t *)(sensors[i].sensor))->lost -
                            sensors[i].last_lost;
                        // setup loss event
                        loss_event.timestamp = sensors[i].last_ts + 1;
                        loss_event.major     = FERRET_EVLOSS_MAJOR;
                        loss_event.minor     = FERRET_EVLOSS_MINOR;
                        loss_event.instance  = 0;
                        loss_event.cpu       = 0;
                        loss_event.data16[0] = sensors[i].major;
                        loss_event.data16[1] = sensors[i].minor;
                        loss_event.data16[2] = sensors[i].instance;
                        loss_event.data64[1] = count;
                        loss_event.data64[2] = list_e_pu->timestamp;

                        ret = buf_copy_event(
                            buf, (ferret_list_entry_t *)&loss_event,
                            sizeof(loss_event), i);

                        // update our copy of the lost_count
                        sensors[i].last_lost =
                            ((ferret_list_moni_t *)(sensors[i].sensor))->lost;

                        if (ret == -1)
                            continue;
                        if (ret == -2)
                        {
                            i = sensor_count;  // leave outer loop
                            break;             // leave inner loop
                        }
                    }
                }
#if 0
                else if (type == FERRET_TBUF)
                {
                    l4_tracebuffer_entry_t tbuf_e;
                    //printf("TBUF:\n");
                    //puts("T");

                    ret = ferret_tbuf_get(
                        (ferret_tbuf_moni_t *)sensors[i].sensor, &tbuf_e);
                    if (ret == -1)
                    {
                        break;
                    }
                    else if (ret < -1)
                    {
                        printf("Something wrong with trace buffer: %d\n", ret);
                    }
                    ferret_util_convert_k2c(&tbuf_e, list_e_pk);
                    size = sizeof(l4_tracebuffer_entry_t);

                    // check lost count here
                    if (((ferret_list_moni_t *)(sensors[i].sensor))->lost >
                        sensors[i].last_lost)
                    {
                        uint64_t count =
                            ((ferret_list_moni_t *)(sensors[i].sensor))->lost -
                            sensors[i].last_lost;
                        // setup loss event
                        loss_event.timestamp = sensors[i].last_ts + 1;
                        loss_event.major     = FERRET_EVLOSS_MAJOR;
                        loss_event.minor     = FERRET_EVLOSS_MINOR;
                        loss_event.instance  = 0;
                        loss_event.cpu       = 0;
                        loss_event.data16[0] = sensors[i].major;
                        loss_event.data16[1] = sensors[i].minor;
                        loss_event.data16[2] = sensors[i].instance;
                        loss_event.data64[1] = count;
                        loss_event.data64[2] = list_e_pk->timestamp;

                        ret = buf_copy_event(
                            buf, (ferret_list_entry_t *)&loss_event,
                            sizeof(loss_event), i);

                        // update our copy of the lost_count
                        sensors[i].last_lost =
                            ((ferret_list_moni_t *)(sensors[i].sensor))->lost;

                        if (ret == -1)
                            continue;
                        if (ret == -2)
                        {
                            i = sensor_count;  // leave outer loop
                            break;             // leave inner loop
                        }
                    }
                }
#endif
                else
                {
                    printf("Found wrong sensor type (%d): %hu!\n", i, type);
                    return 1;
                }

                ret = buf_copy_event(buf, (ferret_list_entry_t *)list_e_pk,
                                     size, i);

                if (ret == -2)
                {
                    i = sensor_count;  // leave outer loop
                    break;             // leave inner loop
                }
            }
        }
//        if (verbose)
//            puts("S");
        gui_update(buf);
        pthread_mutex_unlock(&buf->lock);

        if (global_exit)
        {
            return 0;
        }
        l4_sleep(interval);
    };

    printf("Should not happen!\n");

    return 0;
}
