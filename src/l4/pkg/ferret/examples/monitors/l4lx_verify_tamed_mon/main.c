/**
 * \file   ferret/examples/l4lx_verify_tamed_mon/main.c
 * \brief  Monitor showing atomicity bug in L4Linux' tamer thread
 *
 * \date   2006-08-08
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <l4/log/l4log.h>
#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace_events.h>

#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/list_consumer.h>
#include <l4/ferret/sensors/tbuf_consumer.h>
#include <l4/ferret/maj_min.h>

#include "uu.h"

char LOG_tag[9] = "FerLVTM";

#define HISTORY_SIZE 10000
#define LINE_SIZE    100

typedef struct __attribute__ ((__packed__)) history_entry_s
{
    int                        size;
    ferret_list_entry_common_t event;
} history_entry_t;

history_entry_t history[HISTORY_SIZE];
int history_count    = 0;
int history_position = 0;
int quiet = 0;

static void append_history(ferret_list_entry_common_t * e)
{
    history[history_position].size  = sizeof(ferret_list_entry_common_t);
    history[history_position].event = *e;
    history_position++;
    if (history_position >= HISTORY_SIZE)
        history_position = 0;
    history_count++;
}

static void __attribute__((unused)) append_count(int c)
{
    static int counts[LINE_SIZE];
    static int count = 0;

    counts[count++] = c;
    if (count >= LINE_SIZE)
    {
        for (count = 0; count < LINE_SIZE; count++)
            printf("%d, ", counts[count]);
        printf("\n");
        count = 0;
    }
}

static void eval_event(ferret_list_entry_common_t * e)
{
    //static unsigned short last = -1;
    //static int count_atomic  = 0;
    //static int count_context = 0;

    ferret_list_entry_kernel_t * k = (ferret_list_entry_kernel_t *)e;

    static ferret_utime_t t_start_atomic = 0;
    static ferret_utime_t t_stop_atomic  = 0;
    static ferret_utime_t t_last_context = 0;
    static int l4lx_task_id         = -1;
    static int l4lx_tamer_thread_id = -1;
    static int dump_count = -1;

    switch (e->major)
    {
    case FERRET_L4LX_MAJOR:
        /* debug stuff ...
        if (last != FERRET_L4LX_MAJOR)
        {
            append_count(count_context);
            count_context = 0;
        }
        count_atomic++;
        last = FERRET_L4LX_MAJOR;
        ... debug stuff */
        switch (e->minor)
        {
        case FERRET_L4LX_ATOMIC_BEGIN:
            t_start_atomic = e->timestamp;
            t_stop_atomic  = 0;
            if (l4lx_task_id == -1)  // L4Linux kernel's task id
            {
                l4lx_task_id = ((l4_threadid_t)(e->data32[0])).id.task;
                l4lx_tamer_thread_id =
                    ((l4_threadid_t)(e->data32[0])).id.lthread;
                printf("Tamer TID: %x.%x\n",
                       l4lx_task_id, l4lx_tamer_thread_id);
            }
            break;
        case FERRET_L4LX_ATOMIC_END1:
        case FERRET_L4LX_ATOMIC_END2:
            t_stop_atomic = e->timestamp;
            //printf("%llu:%llu, %llu\n",
            //       t_start_atomic, t_stop_atomic, t_last_context);
            break;
        default:
            //printf("Got other event %hd:%hd:%hd, ignored.\n",
            //       e->major, e->minor, e->instance);
            break;
        }
        break;
    case FERRET_TBUF_MAJOR:
        /* debug stuff ...
        if (last != FERRET_TBUF_MAJOR)
        {
            append_count(count_atomic);
            //printf("Atomic: %d\n", count_atomic);
            count_atomic = 0;
        }
        count_context++;
        last = FERRET_TBUF_MAJOR;
        ... debug stuff */
        switch (e->minor)
        {
        case l4_ktrace_tbuf_context_switch:
            t_last_context = e->timestamp;
            if (t_last_context > t_start_atomic &&
                t_stop_atomic == 0 && t_start_atomic != 0)
            {
                if (l4lx_task_id ==
                    l4_ktrace_get_l4_taskid((void * )k->data32[1]) &&
                    l4lx_tamer_thread_id !=
                    l4_ktrace_get_l4_lthreadid((void * )k->data32[1]))

                {
                    if (quiet < 2 )
                    {
                        printf(
                            l4util_idfmt" -> "l4util_idfmt": " ,
                            l4util_idstr(l4_ktrace_get_l4_threadid(k->context)),
                            l4util_idstr(l4_ktrace_get_l4_threadid(
                                             (void * )k->data32[1]))
                            );
                        printf("Atomicity violation: %llu < %llu\n",
                               t_start_atomic, t_last_context);
                    }
                    dump_count = 100;  // start count down for dump
                }
            }
            break;
        default:
            //printf("Got other event %hd:%hd:%hd, ignored.\n",
            //       e->major, e->minor, e->instance);
            break;
        }
        break;
    default:
        //printf("Got other event %hd:%hd:%hd, ignored.\n",
        //       e->major, e->minor, e->instance);
        break;
    }
    append_history(e);

    if (dump_count > 0)
    {
        dump_count--;
    }
    if (dump_count == 0)
    {
        if (quiet < 1)
        {
            uu_dumpz_ringbuffer("atomicity.dump", &history,
                                HISTORY_SIZE * sizeof(history_entry_t),
                                history_position * sizeof(history_entry_t),
                                HISTORY_SIZE * sizeof(history_entry_t));
        }
        dump_count = -1;
    }
}

static void usage(void)
{
    LOG_printf(
        "usage: fer_l4lx_vtm [-q|-Q]"
        " [-d val]\n"
        "\n"
        "  -q,--quiet ... Be quiet and do not disturb the system with\n"
        "                 long output (also no trace output).  Useful\n"
        "                 for longer running overhead measurements.\n"
        "  -Q,--Quiet ... Be really QUIET and do not disturb the system with\n"
        "                 any output at runtime (also no trace output).  \n"
        "                 Useful for longer running overhead measurements.\n"
        );
}

static int parse_args(int argc, char* argv[])
{
    int optionid;
    int option = 0;
    const struct option long_options[] =
        {
            { "quiet", 0, NULL, 'q'},
            { "Quiet", 0, NULL, 'Q'},
            { 0, 0, 0, 0}
        };

    do
    {
        option = getopt_long(argc, argv, "qQ",
                             long_options, &optionid);
        switch (option)
        {
        case 'q':
            quiet = 1;
            break;
        case 'Q':
            quiet = 2;
            break;
        case -1:  // exit case
            break;
        default:
            printf("error  - unknown option %c\n", option);
            usage();
            return 2;
        }
    } while (option != -1);

    return 0;
}

int main(int argc, char* argv[])
{
    /* 1. startup
     * 2. poll to open all relevant sensors (0:0:0, 1:0:0)
     * 3. in loop get new events and reorder according to time
     * 4. if between atomic begin and atomic end other scheduling
     *    event in same task -> flag error
     */

    int ret;
    ferret_list_moni_t * list = NULL;
    ferret_tbuf_moni_t * tbuf = NULL;
    l4_tracebuffer_entry_t     tbuf_e;   // native tracebuffer entry
    ferret_list_entry_common_t list_e;   // list entry for other sensors
    ferret_list_entry_kernel_t list_te;  // list entry for converted tbuf entry

    printf("History buffer and position addresses: %p, %p\n",
           &history, &history_position);

    if (parse_args(argc, argv))
        return 1;

    // open relevant sensors
    while ((ret = ferret_att(FERRET_TBUF_MAJOR, FERRET_TBUF_MINOR,
                             FERRET_TBUF_INSTANCE, tbuf)))
    {
        printf("Could not attach to %hd:%hd:%hd, retrying soon ...\n",
            FERRET_TBUF_MAJOR, FERRET_TBUF_MINOR, FERRET_TBUF_INSTANCE);
        l4_sleep(1000);
    }
    printf("Attached to %hd:%hd:%hd.\n",
        FERRET_TBUF_MAJOR, FERRET_TBUF_MINOR, FERRET_TBUF_INSTANCE);

    while ((ret = ferret_att(FERRET_L4LX_MAJOR, FERRET_L4LX_LIST_MINOR,
                             1, list)))
    {
        printf("Could not attach to %hd:%hd:%hd, retrying soon ...\n",
            FERRET_L4LX_MAJOR, FERRET_L4LX_LIST_MINOR, 1);
        l4_sleep(1000);
    }
    printf("Attached to %hd:%hd:%hd.\n",
        FERRET_L4LX_MAJOR, FERRET_L4LX_LIST_MINOR, 1);

    // at first, fill the reorder buffers with one element from each sensor
    while (1)
    {
        ret = ferret_tbuf_get(tbuf, &tbuf_e);
        if (ret == -1)
        {
            //LOG("Waiting for element in tbuf ...");
            l4_sleep(1000);
            continue;
        }
        else if (ret < -1)
        {
            printf("Something wrong with tbuf: %d\n", ret);
            exit(1);
        }
        ferret_util_convert_k2c(&tbuf_e, &list_te);
        break;
    }


    while (1)
    {
        ret = ferret_list_get(list, (ferret_list_entry_t *)&list_e);
        if (ret == -1)
        {
            //LOG("Waiting for element in list ...");
            l4_sleep(1000);
            continue;
        }
        else if (ret < -1)
        {
            printf("Something wrong with list: %d\n", ret);
            exit(1);
        }
        break;
    }

    // periodically read from sensors
    while (1)
    {
        if (list_e.timestamp < list_te.timestamp)
        {  // list_e is older, we work with it and get a new one
            eval_event(&list_e);
            while (1)
            {
                ret = ferret_list_get(list, (ferret_list_entry_t *)&list_e);
                if (ret == -1)
                {
                    //LOG("Waiting for element in list ...");
                    l4_sleep(10);
                    continue;
                }
                else if (ret < -1)
                {
                    printf("Something wrong with list: %d\n", ret);
                    exit(1);
                }
                break;
            }
        }
        else
        {  // list_et is older, we work with it and get a new one
            eval_event((ferret_list_entry_common_t *)&list_te);
            while (1)
            {
                ret = ferret_tbuf_get(tbuf, &tbuf_e);
                if (ret == -1)
                {
                    //LOG("Waiting for element in tbuf ...");
                    l4_sleep(10);
                    continue;
                }
                else if (ret < -1)
                {
                    printf("Something wrong with tbuf: %d\n", ret);
                    exit(1);
                }
                ferret_util_convert_k2c(&tbuf_e, &list_te);
                break;
            }
        }
    }

    return 0;
}
