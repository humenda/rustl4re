/**
 * \file   ferret/examples/performance/threads.c
 * \brief  Measure some performance numbers on events
 *
 * \date   2006-04-25
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <limits.h>

#include <l4/log/l4log.h>
#include <l4/thread/thread.h>
#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/semaphore/semaphore.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>
#include <l4/ferret/sensors/ulist_producer.h>
#include <l4/ferret/sensors/slist_producer.h>
#include <l4/ferret/sensors/alist_producer.h>

char LOG_tag[9] = "FerPerf";

#define THREADS 5
#define FAST_POST 1
#define THROUGHPUT_LOOPS 10000000

typedef struct stats_s
{
    l4thread_t    t;
    unsigned int  count;        // overall thread loop counter
    unsigned int  last;         // previous thread loop counter
    unsigned int  current;      // current loop counter
    ferret_time_t start;        // start timestamp
    unsigned int  min;          // minimal time
    unsigned int  max;          // maximal time
    unsigned int  avg;          // sliding average time * 4096
    l4semaphore_t sem;          // semaphore to block threads
    int           should_exit;  // thread should terminate itself
} stats_t;

stats_t stats[THREADS];
ferret_list_local_t * list = NULL;
ferret_alist_t * alist = NULL;
ferret_ulist_local_t * ulist = NULL;
ferret_slist_t * slist = NULL;

const int l4thread_max_threads = 90;

static void thread_fn_list(void *arg)
{
    int me = (int)arg;
    int index;
    ferret_list_entry_common_t * elc;
    unsigned int diff;

    for (stats[me].count = 0; 1; stats[me].count++)
    {
        if ((stats[me].count & 1024) == 0)  // check if we should be running
        {
            l4semaphore_down(&stats[me].sem);
            l4semaphore_up(&stats[me].sem);
            if (stats[me].should_exit)
            {
                l4thread_exit();
            }
        }
        stats[me].start = l4_rdtsc();
        index = ferret_list_dequeue(list);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(list->glob, index);
#ifdef FAST_POST
        elc->maj_min   = MAJ_MIN(12, 2);
#else
        elc->major     = 12;
        elc->minor     = 2;
#endif
        elc->instance  = 0;
        elc->cpu       = 0;
        elc->data32[2] = 1;  // some constant
        elc->data32[3] = stats[me].count;
        ferret_list_commit(list, index);
        diff = l4_rdtsc() - stats[me].start;
        if (diff < stats[me].min)
            stats[me].min = diff;
        else if (diff > stats[me].max)
            stats[me].max = diff;
        // sliding average fixpoint (12 bit shifted)
        stats[me].avg = (31 * stats[me].avg + (diff << 12)) >> 5;
    }
}

static void thread_fn_list2(void *arg)
{
    int me = (int)arg;
    unsigned int diff;

    for (stats[me].count = 0; 1; stats[me].count++)
    {
        if ((stats[me].count & 1024) == 0)  // check if we should be running
        {
            l4semaphore_down(&stats[me].sem);
            l4semaphore_up(&stats[me].sem);
            if (stats[me].should_exit)
            {
                l4thread_exit();
            }
        }
        stats[me].start = l4_rdtsc();
#ifdef FAST_POST
        ferret_list_postX_2w(list, MAJ_MIN(12, 2), 0, 1, stats[me].count);
#else
        ferret_list_post_2w(list, 12, 2, 0, 1, stats[me].count);
#endif
        diff = l4_rdtsc() - stats[me].start;
        if (diff < stats[me].min)
            stats[me].min = diff;
        else if (diff > stats[me].max)
            stats[me].max = diff;
        // sliding average fixpoint (12 bit shifted)
        stats[me].avg = (31 * stats[me].avg + (diff << 12)) >> 5;
    }
}

static void thread_fn_ulist(void *arg)
{
    int me = (int)arg;
    unsigned int diff;

    for (stats[me].count = 0; 1; stats[me].count++)
    {
        if ((stats[me].count & 1024) == 0)  // check if we should be running
        {
            l4semaphore_down(&stats[me].sem);
            l4semaphore_up(&stats[me].sem);
            if (stats[me].should_exit)
            {
                l4thread_exit();
            }
        }
        stats[me].start = l4_rdtsc();
#ifdef FAST_POST
        ferret_ulist_postX_2w(ulist, MAJ_MIN(12, 2), 0, 1, stats[me].count);
#else
        ferret_ulist_post_2w(ulist, 12, 2, 0, 1, stats[me].count);
#endif
        diff = l4_rdtsc() - stats[me].start;
        if (diff < stats[me].min)
            stats[me].min = diff;
        else if (diff > stats[me].max)
            stats[me].max = diff;
        // sliding average fixpoint (12 bit shifted)
        stats[me].avg = (31 * stats[me].avg + (diff << 12)) >> 5;
    }
}

static void thread_fn_slist(void *arg)
{
    int me = (int)arg;
    unsigned int diff;

    for (stats[me].count = 0; 1; stats[me].count++)
    {
        if ((stats[me].count & 1024) == 0)  // check if we should be running
        {
            l4semaphore_down(&stats[me].sem);
            l4semaphore_up(&stats[me].sem);
            if (stats[me].should_exit)
            {
                l4thread_exit();
            }
        }
        stats[me].start = l4_rdtsc();
#ifdef FAST_POST
        ferret_slist_postX_2w(slist, MAJ_MIN(12, 2), 0, 1, stats[me].count);
#else
        ferret_slist_post_2w(slist, 12, 2, 0, 1, stats[me].count);
#endif
        diff = l4_rdtsc() - stats[me].start;
        if (diff < stats[me].min)
            stats[me].min = diff;
        else if (diff > stats[me].max)
            stats[me].max = diff;
        // sliding average fixpoint (12 bit shifted)
        stats[me].avg = (31 * stats[me].avg + (diff << 12)) >> 5;
    }
}

static void thread_fn_alist(void *arg)
{
    int me = (int)arg;
    unsigned int diff;

    for (stats[me].count = 0; 1; stats[me].count++)
    {
        if ((stats[me].count & 1024) == 0)  // check if we should be running
        {
            l4semaphore_down(&stats[me].sem);
            l4semaphore_up(&stats[me].sem);
            if (stats[me].should_exit)
            {
                l4thread_exit();
            }
        }
        stats[me].start = l4_rdtsc();
#ifdef FAST_POST
        ferret_alist_postX_2w(alist, MAJ_MIN(12, 2), 0, 1, stats[me].count);
#else
        ferret_alist_post_2w(alist, 12, 2, 0, 1, stats[me].count);
#endif
        diff = l4_rdtsc() - stats[me].start;
        if (diff < stats[me].min)
            stats[me].min = diff;
        else if (diff > stats[me].max)
            stats[me].max = diff;
        // sliding average fixpoint (12 bit shifted)
        stats[me].avg = (31 * stats[me].avg + (diff << 12)) >> 5;
    }
}


static void reset_stats(int i)
{
    stats[i].min = UINT_MAX;
    stats[i].avg = 200 << 12;
    stats[i].max = 0;
}

static void poll_for_stats(void)
{
    int i, j, threads_active;

    for (threads_active = 1; threads_active <= THREADS; threads_active++)
    {
        LOG("%d threads running now ...", threads_active);
        for (j = 0; j < 2; j++)
        {
            // start some threads and let them run for a little
            for (i = 0; i < threads_active; i++)
            {
                l4semaphore_up(&stats[i].sem);
            }
            l4_sleep(5000);
            for (i = 0; i < threads_active; i++)
            {
                l4semaphore_down(&stats[i].sem);
            }

            // now collect the numbers
            for (i = 0; i < threads_active; i++)
            {
                stats[i].current = stats[i].count;
            }
            for (i = 0; i < threads_active; i++)
            {
                LOG_printf("Thread %d: Cycles/event <%u:%u:%u>, events %u\n",
                           i, stats[i].min, stats[i].avg >> 12, stats[i].max,
                           stats[i].current - stats[i].last);
            }
            for (i = 0; i < threads_active; i++)
            {
                stats[i].last = stats[i].current;
                reset_stats(i);
            }
        }
    }
}

static void test_throughput(void)
{
    unsigned long long start, stop;
    int i;

    start = l4_rdtsc();
    for (i = 0; i < THROUGHPUT_LOOPS; i++)
    {
#ifdef FAST_POST
        ferret_list_postX_2w(list, MAJ_MIN(12, 2), 0, 1, i);
#else
        ferret_list_post_2w(list, 12, 2, 0, 1, i);
#endif
    }
    stop = l4_rdtsc();
    LOG_printf("List throughput: %d loops, %llu cycles, %llu cyc/event\n",
               THROUGHPUT_LOOPS, stop - start,
               (stop - start) / THROUGHPUT_LOOPS);


    start = l4_rdtsc();
    for (i = 0; i < THROUGHPUT_LOOPS; i++)
    {
#ifdef FAST_POST
        ferret_ulist_postX_2w(ulist, MAJ_MIN(12, 2), 0, 1, i);
#else
        ferret_ulist_post_2w(ulist, 12, 2, 0, 1, i);
#endif
    }
    stop = l4_rdtsc();
    LOG_printf("UList throughput: %d loops, %llu cycles, %llu cyc/event\n",
               THROUGHPUT_LOOPS, stop - start,
               (stop - start) / THROUGHPUT_LOOPS);


    start = l4_rdtsc();
    for (i = 0; i < THROUGHPUT_LOOPS; i++)
    {
#ifdef FAST_POST
        ferret_slist_postX_2w(slist, MAJ_MIN(12, 2), 0, 1, i);
#else
        ferret_slist_post_2w(slist, 12, 2, 0, 1, i);
#endif
    }
    stop = l4_rdtsc();
    LOG_printf("SList throughput: %d loops, %llu cycles, %llu cyc/event\n",
               THROUGHPUT_LOOPS, stop - start,
               (stop - start) / THROUGHPUT_LOOPS);


    start = l4_rdtsc();
    for (i = 0; i < THROUGHPUT_LOOPS; i++)
    {
#ifdef FAST_POST
        ferret_alist_postX_2w(alist, MAJ_MIN(12, 2), 0, 1, i);
#else
        ferret_alist_post_2w(alist, 12, 2, 0, 1, i);
#endif
    }
    stop = l4_rdtsc();
    LOG_printf("AList throughput: %d loops, %llu cycles, %llu cyc/event\n",
               THROUGHPUT_LOOPS, stop - start,
               (stop - start) / THROUGHPUT_LOOPS);
}


int main(int argc, char* argv[])
{
    int ret, i;

    l4_sleep(1000);
    // create sensor and configure it to an element size of 64 with
    // 1000 list entries
    ret = ferret_create(12, 2, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:1024", list, &malloc);
    if (ret)
    {
        LOG("Error creating sensor list: ret = %d", ret);
        exit(1);
    }

    ret = ferret_create(12, 3, 0, FERRET_ULIST,
                        FERRET_SUPERPAGES, "64:1024", ulist, &malloc);
    if (ret)
    {
        LOG("Error creating sensor ulist: ret = %d", ret);
        exit(1);
    }

    ret = ferret_create(12, 4, 0, FERRET_SLIST,
                        FERRET_SUPERPAGES, "64:1024", slist, &malloc);
    if (ret)
    {
        LOG("Error creating sensor slist: ret = %d", ret);
        exit(1);
    }

    ret = ferret_create(12, 5, 0, FERRET_ALIST,
                        FERRET_SUPERPAGES, "1024", alist, &malloc);
    if (ret)
    {
        LOG("Error creating sensor alist: ret = %d", ret);
        exit(1);
    }

    test_throughput();

    // list
    // create threads
    for (i = 0; i < THREADS; i++)
    {
        reset_stats(i);
        stats[i].last        = 0;
        stats[i].current     = 0;
        stats[i].sem         = L4SEMAPHORE_LOCKED;
        stats[i].should_exit = 0;
        stats[i].t = l4thread_create(thread_fn_list, (void *)i,
                                     L4THREAD_CREATE_ASYNC);
        if (stats[i].t < 0)
        {
            LOG("Error creating thread %d, ret = %d", i, stats[i].t);
            exit(1);
        }
    }
    LOG("Created %d threads for list benchmark.", i);

    poll_for_stats();

    for (i = 0; i < THREADS; i++)
    {
        stats[i].should_exit = 1;
    }
    l4_sleep(1000);

    // list2
    // create threads
    for (i = 0; i < THREADS; i++)
    {
        reset_stats(i);
        stats[i].last        = 0;
        stats[i].current     = 0;
        stats[i].sem         = L4SEMAPHORE_LOCKED;
        stats[i].should_exit = 0;
        stats[i].t = l4thread_create(thread_fn_list2, (void *)i,
                                     L4THREAD_CREATE_ASYNC);
        if (stats[i].t < 0)
        {
            LOG("Error creating thread %d, ret = %d", i, stats[i].t);
            exit(1);
        }
    }
    LOG("Created %d threads for list2 benchmark.", i);

    poll_for_stats();

    for (i = 0; i < THREADS; i++)
    {
        stats[i].should_exit = 1;
    }
    l4_sleep(1000);


    // ulist
    // create threads
    for (i = 0; i < THREADS; i++)
    {
        reset_stats(i);
        stats[i].last        = 0;
        stats[i].current     = 0;
        stats[i].sem         = L4SEMAPHORE_LOCKED;
        stats[i].should_exit = 0;
        stats[i].t = l4thread_create(thread_fn_ulist, (void *)i,
                                     L4THREAD_CREATE_ASYNC);
        if (stats[i].t < 0)
        {
            LOG("Error creating thread %d, ret = %d", i, stats[i].t);
            exit(1);
        }
    }
    LOG("Created %d threads for ulist benchmark.", i);

    poll_for_stats();

    for (i = 0; i < THREADS; i++)
    {
        stats[i].should_exit = 1;
    }
    l4_sleep(1000);

    // slist
    // create threads
    for (i = 0; i < THREADS; i++)
    {
        reset_stats(i);
        stats[i].last        = 0;
        stats[i].current     = 0;
        stats[i].sem         = L4SEMAPHORE_LOCKED;
        stats[i].should_exit = 0;
        stats[i].t = l4thread_create(thread_fn_slist, (void *)i,
                                     L4THREAD_CREATE_ASYNC);
        if (stats[i].t < 0)
        {
            LOG("Error creating thread %d, ret = %d", i, stats[i].t);
            exit(1);
        }
    }
    LOG("Created %d threads for slist benchmark.", i);

    poll_for_stats();

    for (i = 0; i < THREADS; i++)
    {
        stats[i].should_exit = 1;
    }

    l4_sleep(1000);

    // alist
    // create threads
    for (i = 0; i < THREADS; i++)
    {
        reset_stats(i);
        stats[i].last        = 0;
        stats[i].current     = 0;
        stats[i].sem         = L4SEMAPHORE_LOCKED;
        stats[i].should_exit = 0;
        stats[i].t = l4thread_create(thread_fn_alist, (void *)i,
                                     L4THREAD_CREATE_ASYNC);
        if (stats[i].t < 0)
        {
            LOG("Error creating thread %d, ret = %d", i, stats[i].t);
            exit(1);
        }
    }
    LOG("Created %d threads for alist benchmark.", i);

    poll_for_stats();

    for (i = 0; i < THREADS; i++)
    {
        stats[i].should_exit = 1;
    }

    l4_sleep(1000);


    return 0;
}
