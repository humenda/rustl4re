/**
 * \file   ferret/examples/scalar_demo/main.c
 * \brief  Example demonstrating the usage of scalar sensors with Ferret/RE.
 *         Based on Martin Pohlack's original Ferret package for L4Env.
 *
 * \date   03/04/2009
 * \author Martin Pohlack <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel <doebel@tudos.org>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <stdio.h>

#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/util/util.h>
#include <l4/util/rand.h>

#include <l4/ferret/client.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/scalar.h>
#include <l4/ferret/sensors/scalar_producer.h>
#include <l4/ferret/sensors/scalar_consumer.h>

#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>

int main(void)
{
	printf("Hello from the scalar demo.\n");

	l4_cap_idx_t srv = lookup_sensordir();

    // -=# monitoring code start #=-
    ferret_scalar_t * s1 = NULL, * s2 = NULL, * s3 = NULL, * s4 = NULL;
    int ret, i, j, loops, pm;
    ferret_utime_t start, end;

    l4_sleep(1000);
    // create sensor and configure it to a value range of 0 -- 100 and
    // the unit '%'
    ret = ferret_create(srv, 10, 1, 0, FERRET_SCALAR,
                        0, "0:100:%", s1, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to a value range of 0 -- 1000 and
    // the unit 'time [us]'
    ret = ferret_create(srv, 10, 2, 0, FERRET_SCALAR,
                        0, "0:1000:time [us]", s2, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to a value range of 0 -- 100000 and
    // the unit 'progress'
    ret = ferret_create(srv, 10, 3, 0, FERRET_SCALAR,
                        0, "0:1000:progress", s3, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }

    // create sensor and configure it to a value range of 0 -- 100000 and
    // the unit 'Bandwidth [MB/s]'
    ret = ferret_create(srv, 10, 4, 0, FERRET_SCALAR,
                        0, "0:67:Bandwidth [MB/s]", s4, &malloc);
    if (ret)
    {
        printf("Error creating sensor: ret = %d\n", ret);
        exit(1);
    }
    // -=# monitoring code end #=-

    pm = 0;
    j = 0;

	printf("-= Done creating sensors. Starting to produce data.=-\n");

    while (1)
    {
        pm = (pm + 1) % 1000;
        // -=# monitoring code start #=-
        // simply insert a local value to transport it to a monitor
        ferret_scalar_put(s1, pm / 10);
        // -=# monitoring code end #=-

        // -=# monitoring code start #=-
        FERRET_GET_TIME(FERRET_TIME_REL_US, start);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + l4util_rand();
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");

        // -=# monitoring code start #=-
        // measure some duration and transport it to a monitor
        //FERRET_GET_TIME(FERRET_TIME_REL_US_FAST, end); // does not work
                                                           // with fiasco-ux
        FERRET_GET_TIME(FERRET_TIME_REL_US, end);
        ferret_scalar_put(s2, end - start);
        // -=# monitoring code end #=-

        // -=# monitoring code start #=-
        // another simple value transported
        ferret_scalar_put(s3, j);
        // -=# monitoring code end #=-

        // -=# monitoring code start #=-
        // make some internal statistic available externally
        // bandwidth will be between 30 and 65 MB/s
        ferret_scalar_put(s4, 30  + l4util_rand() % 35);
        // -=# monitoring code end #=-

        l4_sleep(10);
        j++;
    }

    // -=# monitoring code start #=-
    /* will never be reached due to the infinite loop above, but
     * demonstrates cleanup.
     *
     * The sensor will be freed after all parties released it, that
     * is, the creator and all monitors.
     */
    ret = ferret_free_sensor(10, 1, 0, s1, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }

    ret = ferret_free_sensor(10, 2, 0, s2, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }

    ret = ferret_free_sensor(10, 3, 0, s3, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }

    ret = ferret_free_sensor(10, 4, 0, s4, &free);
    if (ret)
    {
        printf("Error freeing sensor: ret = %d\n", ret);
        exit(1);
    }

    // -=# monitoring code end #=-

    return 0;
}
