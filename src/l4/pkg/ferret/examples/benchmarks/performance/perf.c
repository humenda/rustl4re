/**
 * \file   ferret/examples/performance/perf.c
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
#include <stdio.h>

#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/list_producer.h>

int main(void)
{
    ferret_list_local_t * l1 = NULL, * l2 = NULL, * l3 = NULL;
	l4_cap_idx_t srv = lookup_sensordir();
    int ret;
    int i, j;
    int index;
    ferret_time_t t1, t2;

    ferret_list_entry_common_t * elc;

    l4_sleep(1000);
    // create sensor and configure it to an element size of 8 with
    // 100 list entries
    ret = ferret_create(srv, 12, 1, 0, FERRET_LIST,
                        0, "8:128", l1, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to an element size of 64 with
    // 1000 list entries
    ret = ferret_create(srv, 12, 2, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:1024", l2, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to an element size of 64 with
    // 65000 list entries
    ret = ferret_create(srv, 12, 3, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:65000", l3, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    printf("---------- 1,000,000 loops\n");

    for (j = 0; j < 10; j++)
    {
        t1 = l4_rdtsc();
        for (i = 0; i < 1000000; i++)
        {
            index = ferret_list_dequeue(l1);
            ferret_list_commit(l1, index);
        }
        t2 = l4_rdtsc();
        printf("l1: %lld cycles per loop!\n",
			
            (t2 - t1) / 1000000);
        l4_sleep(1000);
    }

    printf("---------- 1,000,000 loops\n");

    for (j = 0; j < 10; j++)
    {
        t1 = l4_rdtsc();
        for (i = 0; i < 1000000; i++)
        {
            index = ferret_list_dequeue(l2);
            ferret_list_commit(l2, index);
        }
        t2 = l4_rdtsc();
        printf("l2: %lld cycles per loop!\n",
            (t2 - t1) / 1000000);
        l4_sleep(1000);
    }

    printf("---------- 1,000,000 loops\n");

    for (j = 0; j < 10; j++)
    {
        t1 = l4_rdtsc();
        for (i = 0; i < 1000000; i++)
        {
            index = ferret_list_dequeue(l2);
            elc = (ferret_list_entry_common_t *)
                ferret_list_e4i(l2->glob, index);
            elc->major     = 12;
            elc->minor     = 2;
            elc->instance  = 0;
            elc->cpu       = 0;
            elc->data32[2] = 1;  // start
            elc->data32[3] = j;
            ferret_list_commit(l2, index);
        }
        t2 = l4_rdtsc();
        printf("l2 (+ payload): %lld cycles per loop!\n",
            (t2 - t1) / 1000000);
        l4_sleep(1000);
    }

    printf("---------- 1,000,000 loops\n");

    for (j = 0; j < 10; j++)
    {
        t1 = l4_rdtsc();
        for (i = 0; i < 1000000; i++)
        {
            index = ferret_list_dequeue(l3);
            ferret_list_commit(l3, index);
        }
        t2 = l4_rdtsc();
        printf("l3: %lld cycles per loop!\n",
            (t2 - t1) / 1000000);
        l4_sleep(1000);
    }

    printf("---------- 1,000,000 loops\n");

    for (j = 0; j < 10; j++)
    {
        t1 = l4_rdtsc();
        for (i = 0; i < 1000000; i++)
        {
            index = ferret_list_dequeue(l3);
            elc = (ferret_list_entry_common_t *)
                ferret_list_e4i(l3->glob, index);
            elc->major     = 12;
            elc->minor     = 2;
            elc->instance  = 0;
            elc->cpu       = 0;
            elc->data32[2] = 1;  // start
            elc->data32[3] = j;
            ferret_list_commit(l3, index);
        }
        t2 = l4_rdtsc();
        printf("l3 (+ payload): %lld cycles per loop!\n",
            (t2 - t1) / 1000000);
        l4_sleep(1000);
    }

    return 0;
}
