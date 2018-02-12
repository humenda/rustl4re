/**
 * \file   ferret/examples/dp_test/main.c
 * \brief  Testcase for delayed preemption lists.
 *
 * \date   2007-05-21
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

#include <l4/log/l4log.h>
#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/monitor.h>
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>
#include <l4/ferret/sensors/list_consumer.h>
#include <l4/ferret/sensors/dplist.h>
#include <l4/ferret/sensors/dplist_producer.h>
#include <l4/ferret/sensors/dplist_consumer.h>

#define LOOPS       (128 * 1024 * 1024)
#define LOOPS_INNER 1024

char LOG_tag[9] = "FerDPTest";

int main(int argc, char* argv[])
{
    int i, j, ret;
    long long ts[2];

    // -=# monitoring code start #=-
    ferret_dplist_t * dpl1 = NULL;
    ferret_list_local_t * l1 = NULL;

    ferret_list_moni_t * ml1 = NULL;
    ferret_dplist_moni_t * mdpl1 = NULL;

    ferret_list_entry_t * el = malloc(64);

    l4_sleep(1000);
    // create sensor and configure it to an element size of 64 with
    // 1024 list entries
    ret = ferret_create(2, 3, 4, FERRET_DPLIST, 0, "64:1024", dpl1, &malloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }
    //LOG("DPList created with the following properties:"
    //    " count = %d, element_size = %d", dpl1->count, dpl1->element_size);

    // create sensor and configure it to an element size of 64 with
    // 1024 list entries
    ret = ferret_create(3, 3, 4, FERRET_LIST, 0, "64:1024", l1, &malloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }

    // self attach to sensors to read stuff from them again
    ret = ferret_att(2, 3, 4, mdpl1);
    if (ret)
    {
        LOG("Could not attach to sensor 2:3:4");
        exit(1);
    }
    ret = ferret_att(3, 3, 4, ml1);
    if (ret)
    {
        LOG("Could not attach to sensor 3:3:4");
        exit(1);
    }
    // -=# monitoring code end #=-

    printf("Beginning tests, stay tuned ...\n");
    printf("Doing %d writes:\n", LOOPS);
    ts[0] = l4_rdtsc();
    for (i = 0; i < LOOPS; i++)
    {
        ferret_dplist_post_1w(dpl1, 2, 3, 4, i);
    }
    ts[1] = l4_rdtsc();
    printf("DPList: cycles per event = %g\n", (ts[1] - ts[0]) / (double)LOOPS);

    ts[0] = l4_rdtsc();
    for (i = 0; i < LOOPS; i++)
    {
        ferret_list_post_1w(l1, 2, 3, 4, i);
    }
    ts[1] = l4_rdtsc();
    printf("List:   cycles per event = %g\n", (ts[1] - ts[0]) / (double)LOOPS);

    printf("Doing %d write--read pairs %d times:\n",
           LOOPS_INNER, LOOPS / LOOPS_INNER);
    ts[0] = l4_rdtsc();
    for (j = 0; j < LOOPS / LOOPS_INNER; j++)
    {
        //printf("b lost %lld events\n", mdpl1->lost);
        for (i = 0; i < LOOPS_INNER; i++)
        {
            ferret_dplist_post_1w(dpl1, 2, 3, 4, i);
        }
        for (i = 0; i < LOOPS_INNER; i++)
        {
            ret = ferret_dplist_get(mdpl1, el);
            if (ret != 0)
            {
                printf("Error code from ferret_dplist_get(): %d\n", ret);
                printf("head = %lld, next_read = %lld, i = %d\n",
                       mdpl1->glob->head.value, mdpl1->next_read.value, i);
                exit(1);
            }
        }
        //printf("a lost %lld events\n", mdpl1->lost);
    }
    ts[1] = l4_rdtsc();
    printf("DPList: lost %lld events\n", mdpl1->lost);
    printf("DPList: cycles per event (write + read) = %g\n",
           (ts[1] - ts[0]) / (double)LOOPS);

    ts[0] = l4_rdtsc();
    for (j = 0; j < LOOPS / LOOPS_INNER; j++)
    {
        for (i = 0; i < LOOPS_INNER; i++)
        {
            ferret_list_post_1w(l1, 2, 3, 4, i);
        }
        for (i = 0; i < LOOPS_INNER; i++)
        {
            ret = ferret_list_get(ml1, el);
            if (ret != 0)
            {
                printf("Error code from ferret_dplist_get(): %d\n", ret);
                exit(1);
            }
        }
    }
    ts[1] = l4_rdtsc();
    printf("List:   lost %lld events\n", ml1->lost);
    printf("List:   cycles per event (write + read) = %g\n",
           (ts[1] - ts[0]) / (double)LOOPS);


#if 0
    j = 0;
    while (1)
    {
        // -=# monitoring code start #=-
        ferret_dplist_post_2w(dpl1, 2, 3, 4, j, 0);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + l4util_rand();
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");  // memory barrier

        // -=# monitoring code start #=-
        ferret_dplist_post_2w(dpl1, 2, 3, 4, j, 1);
        // -=# monitoring code end #=-

        l4_sleep(100);
        j++;
        if (j % 10 == 0)
        {
            LOG(".");
        }
    }
#endif

    // -=# monitoring code start #=-
    /* Demonstrates cleanup.
     *
     * The sensor will be freed after all parties released it, that
     * is, the creator and all monitors.
     */
    ret = ferret_free_sensor(2, 3, 4, dpl1, &free);
    if (ret)
    {
        LOG("Error freeing sensor: ret = %d", ret);
        exit(1);
    }
    // -=# monitoring code end #=-

    return 0;
}
