/**
 * \file   ferret/examples/pthread_test/main.c
 * \brief  Test for measuring sensor overhead in multithreaded environment.
 *
 * \date   2007-05-14
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/times.h>
#include <sys/time.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>
#include <l4/ferret/sensors/list_init.h>
/*
#include <l4/ferret/l4lx_client.h>
#include <l4/ferret/util.h>
*/

#define MAX_THREADS 16
#define LIST_CONFIG "64:10000"

int threads;
pthread_t thread_ids[MAX_THREADS];
ferret_list_t * list;
ferret_list_local_t * llist;

static void * do_work(void * p)
{
    int i;
    for (i = 0; i < 2000000; i++)
    {
        ferret_list_post_1wc(1, llist, 1, 2, 3, 0x12345678);
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int ret, i;
    struct timeval tv1, tv2;
    long long temp;

    threads = 16;

    /* 1. setup sensors in local memory
     * 2. setup some local threads
     * 3. let threads work in local sensor, measuring overhead
     * 4. join threads
     */


    // 1.
    {
        ssize_t size;
        size = ferret_list_size_config(LIST_CONFIG);
        if (size <= 0)
        {
            fprintf(stderr, "Error getting size for list.\n");
            exit(1);
        }
        ret = posix_memalign((void **)&list, 4096, size);
        if (list == NULL || ret != 0)
        {
            fprintf(stderr, "Error getting memory for list.\n");
            exit(1);
        }
        ret = ferret_list_init(list, LIST_CONFIG);
        if (ret != 0)
        {
            fprintf(stderr, "Something wrong in list_init: %d, %s.\n",
                    ret, LIST_CONFIG);
            exit(1);
        }
        llist = (ferret_list_local_t *)list;
        ferret_list_init_producer((void **)&llist, &malloc);
    }

    ret = gettimeofday(&tv1, NULL);

    // 2.
    for (i = 0; i < threads; i++)
    {
        ret = pthread_create(&thread_ids[i], NULL, do_work, (void *)i);
        if (ret != 0)
        {
            perror("pthread_create");
            exit(1);
        }
    }

    // 3.
    // empty

    // 4.
    for (i = 0; i < threads; i++)
    {
        ret = pthread_join(thread_ids[i], NULL);
        if (ret != 0)
        {
            perror("pthread_join");
            exit(1);
        }
    }
    ret = gettimeofday(&tv2, NULL);
    temp = tv2.tv_sec * 1000000LL + tv2.tv_usec -
           tv1.tv_sec * 1000000LL + tv1.tv_usec;
    printf("Time [s]: %f\n", temp / 1000000.0);

    return 0;
}
