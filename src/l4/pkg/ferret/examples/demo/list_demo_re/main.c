/**
 * \file   ferret/examples/list_demo/main.c
 * \brief  Example demonstrating the usage of lists.
 *
 * \date   25/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel   <doebel@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>
#include <l4/util/util.h>
#include <l4/util/rand.h>

#include <stdlib.h>
#include <stdio.h>

#include <l4/sys/vcon.h>

int main(void)
{
	l4_cap_idx_t srv = lookup_sensordir();
    int i, j, loops;


    // -=# monitoring code start #=-
    ferret_list_local_t * l1 = NULL, * l2 = NULL, * l3 = NULL, * l4 = NULL;
    int ret;
    int index;
    ferret_utime_t t;

    ferret_list_entry_common_t * elc;
    ferret_list_entry_t * el;

    l4_sleep(1000);
    // create sensor and configure it to an element size of 8 bytes with
    // 100 list entries
    ret = ferret_create(srv, 12, 1, 0, FERRET_LIST,
                        0, "8:100", l1, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to an element size of 64 bytes with
    // 1000 list entries
    ret = ferret_create(srv, 12, 2, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:1000", l2, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }


    // testing reopen here
    ret = ferret_create(srv, 12, 2, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:1000", l3, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }
    else
    {
        printf("Reopen worked.\n");
    }

    ret = ferret_create(srv, 12, 2, 0, FERRET_LIST,
                        FERRET_SUPERPAGES, "64:999", l4, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d (as expected)\n", ret);
    }
    else
    {
        printf("Reopen worked (should not).\n");
        exit(1);
    }

    // -=# monitoring code end #=-

    j = 0;
    while (1)
    {
        // -=# monitoring code start #=-
        // this sensor has empty events, just timestamps are inserted
        // on commiting, so we don't have to insert other data
        //printf("X1: %p, %p, %p, %p\n", l1, l1->glob, l1->ind_buf, l1->out_buf);
        index = ferret_list_dequeue(l1);
        ferret_list_commit(l1, index);
        index = ferret_list_dequeue(l1);
        ferret_list_commit(l1, index);
        index = ferret_list_dequeue(l1);
        ferret_list_commit(l1, index);
        index = ferret_list_dequeue(l1);
        ferret_list_commit(l1, index);
        // -=# monitoring code end #=-

        // -=# monitoring code start #=-
        // this demonstrates to cast the received pointer to a certain
        // struct and fill it
        index = ferret_list_dequeue(l2);
        elc = (ferret_list_entry_common_t *)ferret_list_e4i(l2->glob, index);
        FERRET_GET_TIME(FERRET_TIME_REL_US, elc->data64[0]);
        elc->major     = 12;
        elc->minor     = 2;
        elc->instance  = 0;
        elc->cpu       = 0;
        elc->data32[2] = 1;  // start
        elc->data32[3] = j;
        ferret_list_commit(l2, index);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + l4util_rand();
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");

        // -=# monitoring code start #=-
        // here, dynamic marshaling is demonstrated
        index = ferret_list_dequeue(l2);
        el = ferret_list_e4i(l2->glob, index);
        FERRET_GET_TIME(FERRET_TIME_REL_US, t);
        //printf("%lld, %p, %p\n", t, el, l2->glob);
        ret = ferret_util_pack("hhhbxqll", el->data, 12, 2, 0, 0, t, 2, j);
        ferret_list_commit(l2, index);
        // -=# monitoring code end #=-

        // -=# monitoring code start #=-
        // demonstrate use of convenience wrappers here
        ferret_list_post_c  (1, l3, 12, 2, 0);
        ferret_list_post_1wc(1, l3, 12, 2, 0, 1);
        ferret_list_post_2wc(1, l3, 12, 2, 0, 1, 2);
        ferret_list_post_3wc(1, l3, 12, 2, 0, 1, 2, 3);
        ferret_list_post_4wc(1, l3, 12, 2, 0, 1, 2, 3, 4);
        // -=# monitoring code end #=-

        l4_sleep(100);
        j++;
    }

    // -=# monitoring code start #=-
    /* will never be reached due to the infinite loop above, but
     * demonstrates cleanup.
     *
     * The sensor will be freed after all parties released it, that
     * is, the creator and all monitors.
     */
    ret = ferret_free_sensor(10, 1, 0, l1, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }

    ret = ferret_free_sensor(10, 2, 0, l2, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }
    // -=# monitoring code end #=-
    return 0;
}
