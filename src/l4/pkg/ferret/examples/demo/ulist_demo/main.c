/**
 * \file   ferret/examples/ulist_demo/main.c
 * \brief  Example demonstrating the usage of ulists.
 *
 * \date   2007-06-21
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
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
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/ulist_producer.h>

char LOG_tag[9] = "FerULiDe";

int main(int argc, char* argv[])
{
    int i, j, loops;

    // -=# monitoring code start #=-
    ferret_ulist_local_t * l1 = NULL;
    int ret;

    l4_sleep(1000);
    // create sensor and configure it to an element size of 64 with
    // 1000 list entries
    ret = ferret_create(12, 2, 0, FERRET_ULIST,
                        FERRET_SUPERPAGES, "64:1000", l1, &malloc);
    if (ret)
    {
        LOG("Error creating sensor: ret = %d", ret);
        exit(1);
    }
    // -=# monitoring code end #=-

    j = 0;
    while (1)
    {
        // -=# monitoring code start #=-
        ferret_ulist_post_1w(l1, 1, 2, 3, 0xabcd1234);
        // -=# monitoring code end #=-

        // do some work
        loops = 10000 + l4util_rand();
        for (i = 0; i < loops; i++)
            asm volatile ("": : : "memory");

        // -=# monitoring code start #=-
        ferret_ulist_post_1w(l1, 1, 2, 3, 0xabcd5678);
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
    ret = ferret_free_sensor(12, 2, 0, l1, &free);
    if (ret)
    {
        LOG("Error freeing sensor: ret = %d", ret);
        exit(1);
    }
    // -=# monitoring code end #=-

    return 0;
}
