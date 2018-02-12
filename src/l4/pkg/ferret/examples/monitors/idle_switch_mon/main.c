/**
 * \file   ferret/examples/idle_switch_mon/main.c
 * \brief  Consumes kernel tracebuffer context switch events and
 *         computes statistics about idle times,
 *         [x -> idle -> x] vs. [x -> idle -> y] context switches
 *         sequences etc.
 *
 * \date   2007-06-04
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <getopt.h>

#include <l4/log/l4log.h>
#include <l4/thread/thread.h>
#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/util/rdtsc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/client.h>
#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/maj_min.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/tbuf_consumer.h>
#include <l4/ferret/sensors/histogram.h>
#include <l4/ferret/sensors/histogram_producer.h>
#include <l4/ferret/sensors/histogram_consumer.h>

// states for state machine
#define STATE_IN_IDLE  0
#define STATE_IN_OTHER 1

char LOG_tag[9] = "FerIdMo";

/* Some magic for converting context pointers to thread_ids
 * We cannot use l4sys' stuff here, because it does not handle 4k TCBs
 */
#define MY_TCB_MAGIC 0xC0000000

static unsigned task_shift   = 18;
static unsigned thread_shift = 11;
static unsigned thread_mask  = (1 << 7) - 1;

static inline l4_threadid_t __get_l4_threadid(Context *context)
{
    l4_threadid_t id;
    id.raw        = 0;
    id.id.task    = (((unsigned) context) - MY_TCB_MAGIC) >> task_shift;
    id.id.lthread = ((((unsigned) context) - MY_TCB_MAGIC) >> thread_shift) &
                    thread_mask;
    return id;
}

long long switch_count = 0, thread_switch_equal = 0, task_switch_equal = 0;
unsigned long long initial_lost, current_lost;
ferret_histo_t * idle_histo = NULL;
int start_demo_thread = 0;
int loop_count = 10000;
char * histo_setup = NULL;

static void working(void *arg)
{
    int i;

    while (1)
    {
        for (i = 0; i < 1000000; i++)
        {
        }
        l4_sleep(10);
    }
}


static void
update_statistics(int equal_thread, int equal_task, ferret_time_t idle,
                  unsigned long long current_lost)
{
    static int call_count = 0;
    static ferret_time_t idle_us;
    static unsigned long long last_lost = 0;

    call_count++;

    if (equal_thread)
    {
        thread_switch_equal++;
    }
    if (equal_task)
    {
        task_switch_equal++;
    }
    switch_count++;

    // fixme: update timing histogram etc.
    idle_us = l4_tsc_to_us(idle);
    ferret_histo_inc(idle_histo, idle_us);

    if (call_count >= loop_count)
    {
        call_count = 0;
        printf("Idle thread switches (all, thread, task): "
               "%llu, %llu  (%llu%%), %llu  (%llu%%)\n",
               switch_count,
               thread_switch_equal,
               (thread_switch_equal * 100) / switch_count,
               task_switch_equal,
               (task_switch_equal * 100) / switch_count);
        printf("Lost %llu events in this period.\n", current_lost - last_lost);
        last_lost = current_lost;
        /*printf("Idle time (min, avg, max): (%llu, %llu, %llu)\n",
               idle_histo->head.val_min,
               idle_histo->head.val_sum / idle_histo->head.val_count,
               idle_histo->head.val_max);*/
        ferret_histo_dump_smart(idle_histo);
    }
}

static void usage(void)
{
    printf(
    "usage: fer_idle_mon [options]\n"
    "\n"
    "  -4,--4k .......... Adapt context pointer computation to to 4K kernel\n"
    "                     stacks.\n"
    "  -d,--demo ........ Start a dummy thread in the background to cause\n"
    "                     some context switches.\n"
    "  -s,--histo_size .. Override histogram setup for idle times.\n"
    "  -l,--loop ........ Print stats every nth update (default 10000).\n"
    "  -h,--help ........ Print this help screen and exit.\n"
    "\n"
    "Example: fer_idle_mon -4 -d -s \"0:1000000:10000\"\n"
    "This will collection information about the running system regarding idle"
    " thread context switch patterns and idle thread activity times and startup"
    " a demo thread.  The histogram will be configured for times between 0 and"
    " 1000000 usecs with 10000 bins, i.e., a resolution of 100 usecs.\n"
    "\n"
    "Note: For this to make sense you should activate context switch trace"
    " buffer events in JDB (O0+).\n"
    );
}

static void parse_cmdline(int argc, char* argv[])
{
    int optionid;
    int option = 0;
    const struct option long_options[] =
        {
            { "4k",         0, NULL, '4'},
            { "help",       0, NULL, 'h'},
            { "demo",       0, NULL, 'd'},
            { "histo_size", 1, NULL, 's'},
            { "loop",       1, NULL, 'l'},
            { 0, 0, 0, 0}
        };

    do
    {
        option = getopt_long(argc, argv, "4hds:l:", long_options, &optionid);
        switch (option)
        {
        case '4':  // adapt layout of context address to 4K kernel stacks
            task_shift   = 19;
            thread_shift = 12;
            break;
        case 'd':
            start_demo_thread = 1;
            break;
        case 'l':
            loop_count = atoi(optarg);
            if (loop_count < 1 || loop_count > 1000000)
            {
                printf("error  - bad value for loop_count %c, should "
                       "be 1..1000000\n", loop_count);
                exit(1);
            }
            break;
        case 's':
            histo_setup = strdup(optarg);
            break;
        case 'h':
            usage();
            exit(1);
            break;
        case -1:  // exit case
            break;
        default:
            printf("error  - unknown option %c\n", option);
            usage();
            exit(2);
        }
    } while (option != -1);
}

int main(int argc, char* argv[])
{
    int ret;
    ferret_tbuf_moni_t * tbuf;
    l4_tracebuffer_entry_t te;

    // histogram from 0 sec to 1 sec with a bin count of 1000000 ->
    // bin size is 1 usec
    histo_setup = "0:1000000:1000000";
    parse_cmdline(argc, argv);

    // attach to tracebuffer
    ret = ferret_att(FERRET_TBUF_MAJOR, FERRET_TBUF_MINOR,
                     FERRET_TBUF_INSTANCE, tbuf);
    if (ret)
    {
        LOG("Could not attach to tracebuffer, fatal!");
        exit(1);
    }
    initial_lost = tbuf->lost;
    LOG("Initial lost count for tracebuffer is %llu.", initial_lost);

    // create a histogram for idle times
    ret = ferret_create(30, 1, 0, FERRET_HISTO, 0, histo_setup,
                        idle_histo, &malloc);
    if (ret)
    {
        LOG("Could not create idle time histogram, fatal!");
        exit(1);
    }

    // create other thread to cause some context switches
    if (start_demo_thread)
    {
        l4thread_create_named(working, ".worker", 0,
                              L4THREAD_CREATE_ASYNC);
    }

    /* 1.   get tbuf event
     * 1.1  if none available -> sleep a little bit -> 1.
     * 2.   if not context switch event -> 1.
     * 3.   if state == IN_OTHER && e.dest == IDLE
     * 3.1     save e.from
     * 3.2     save e.ts
     * 4.   if state == IN_IDLE &&  e.dest != IDLE
     * 4.1     save e.dest
     * 4.2     save e.ts
     * 4.3     update_stats()
     * 5.   -> 1.
     */

    int state = STATE_IN_OTHER;  // init. with unknown state
    ferret_time_t ts_to_idle, ts_from_idle;
    l4_threadid_t tid_to_idle, tid_from_idle, temp;

    // periodically read from sensor
    while (1)
    {
        ret = ferret_tbuf_get(tbuf, &te);              // 1.
        current_lost = tbuf->lost;
        if (ret == -1)
        {
            l4_sleep(1000);
            continue;
        }
        else if (ret < -1)
        {
            LOG("Something wrong with trace buffer: %d", ret);
            exit(1);
        }

        if (te.type != l4_ktrace_tbuf_context_switch)  // 2.
        {
            LOG("Got other event type: %hhu", te.type);
            continue;
        }

        //temp = __get_l4_threadid(te.context);
        //LOG("From: %u.%u", temp.id.task, temp.id.lthread);
        switch (state)
        {
        case STATE_IN_OTHER:                           // 3.
            temp = __get_l4_threadid(te.m.context_switch.dest);
            if (temp.raw != 0)  // idle thread
            {
                //LOG("From: %u.%u", temp.id.task, temp.id.lthread);
                continue;
            }
            //LOG("O+");
            ts_to_idle  = te.tsc;
            tid_to_idle = __get_l4_threadid(te.context);
            state = STATE_IN_IDLE;
            break;
        case STATE_IN_IDLE:                            // 4.
            temp = __get_l4_threadid(te.m.context_switch.dest);
            if (temp.raw == 0)  // idle thread
            {
                continue;
            }
            //LOG("I+");
            ts_from_idle  = te.tsc;
            tid_from_idle = temp;
            state = STATE_IN_OTHER;
            update_statistics(tid_to_idle.raw == tid_from_idle.raw,
                              tid_to_idle.id.task == tid_from_idle.id.task,
                              ts_from_idle - ts_to_idle, current_lost);
            break;
        default:
            LOG("Eww.");
            exit(1);
        }

    }

    return 0;
}
