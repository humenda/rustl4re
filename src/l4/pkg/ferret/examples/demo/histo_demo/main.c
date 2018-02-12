/**
 * \file   ferret/examples/histo_demo/main.c
 * \brief  Example demonstrating the usage of histograms.
 *
 * \date   21/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

#include <l4/log/l4log.h>
#include <l4/util/l4_macros.h>
#include <l4/util/rand.h>
#include <l4/util/util.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>

#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/histogram_producer.h>

char LOG_tag[9] = "FerHiDe";

int main(int argc, char* argv[])
{
    int i, j, loops;

    // -=# monitor code start #=-
    ferret_histo_t * h1 = NULL;
    int ret;
    ferret_utime_t start, end;

    l4_sleep(1000);
    // create histogram and configure it to a value range of 0 --
    // 200000 with 10 bins
    ret = ferret_create(11, 1, 0, FERRET_HISTO,
                        0, "0:200000:10", h1, &malloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }
    // -=# monitor code end #=-

    for (j = 0;; ++j)
    {
        // -=# monitoring code start #=-
        FERRET_GET_TIME(FERRET_TIME_REL_TSC_FAST, start);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + l4util_rand();
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");

        // -=# monitoring code start #=-
        FERRET_GET_TIME(FERRET_TIME_REL_TSC_FAST, end);
        ferret_histo_inc(h1, end - start);
        // -=# monitoring code end #=-

        // be nice to the system
        if (j % 100 == 0)
            l4_sleep(10);
    }

    // -=# monitoring code start #=-
    /* will never be reached due to the infinite loop above, but
     * demonstrates cleanup.
     */
    ret = ferret_free_sensor(11, 1, 0, h1, &free);
    if (ret)
    {
        LOG("Error freeing sensor: ret = %d", ret);
        exit(1);
    }
    // -=# monitoring code end #=-

    return 0;
}
