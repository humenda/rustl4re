/**
 * \file   ferret/examples/simple_mon/main.c
 * \brief  Example demonstrating the usage of monitoring functions.
 *
 * \date   11/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
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

#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/sensors/scalar_consumer.h>
#include <l4/ferret/sensors/histogram_consumer.h>
#include <l4/ferret/sensors/list_consumer.h>
#include <l4/ferret/sensors/tbuf_consumer.h>

char LOG_tag[9] = "FerSiMo";

int main(int argc, char* argv[])
{
    int c, i, ret;
    const int max_count = 100;
    ferret_scalar_t * s1, * s2, * s3, * s4;
    ferret_histo_t * h1;
    ferret_list_moni_t * l1, * l2;
    ferret_tbuf_moni_t * t1;
    ferret_monitor_list_entry_t entries[max_count];
    ferret_monitor_list_entry_t *e = entries;
    l4_tracebuffer_entry_t te[100];
    int te_count;
    ferret_time_t told;

    ferret_list_entry_t * le = malloc(sizeof(ferret_list_entry_t) +
                                       sizeof(ferret_list_entry_common_t));
    ferret_list_entry_common_t * lec;


    l4_sleep(2000);

    // listing the directories content
    c = ferret_list(&e, max_count, 0);
    for (i = 0; i < c; i++)
    {
        LOG("%hu, %hu, %hu, %hu, %u", entries[i].major, entries[i].minor,
            entries[i].instance, entries[i].type, entries[i].id);
    }

    c = ferret_list(&e, 2, 0);
    for (i = 0; i < c; i++)
    {
        LOG("%hu, %hu, %hu, %hu, %u", entries[i].major, entries[i].minor,
            entries[i].instance, entries[i].type, entries[i].id);
    }
    c = ferret_list(&e, 2, 2);
    for (i = 0; i < c; i++)
    {
        LOG("%hu, %hu, %hu, %hu, %u", entries[i].major, entries[i].minor,
            entries[i].instance, entries[i].type, entries[i].id);
    }

    // attaching to some sensors

    // scalars ...
    ret = ferret_att(10, 1, 0, s1);
    if (ret)
    {
        LOG("Could not attach to 10:1, ignored");
    }
    ret = ferret_att(10, 2, 0, s2);
    if (ret)
    {
        LOG("Could not attach to 10:2, ignored");
    }
    ret = ferret_att(10, 3, 0, s3);
    if (ret)
    {
        LOG("Could not attach to 10:3, ignored");
    }
    ret = ferret_att(10, 4, 0, s4);
    if (ret)
    {
        LOG("Could not attach to 10:4, ignored");
    }

    // histograms ...
    ret = ferret_att(11, 1, 0, h1);
    if (ret)
    {
        LOG("Could not attach to 11:1, ignored");
    }

    // lists ...
    ret = ferret_att(12, 1, 0, l1);
    if (ret)
    {
        LOG("Could not attach to 12:1, ignored");
    }

    ret = ferret_att(12, 2, 0, l2);
    if (ret)
    {
        LOG("Could not attach to 12:2, ignored");
    }

    // tracebuffer
    ret = ferret_att(0, 0, 0, t1);
    if (ret)
    {
        LOG("Could not attach to 0:0, ignored");
    }

    // periodically read from sensors and dump their values
    while (1)
    {
        puts("************************************************************\n");
        if (s1)
            printf("s1: %lld, ", ferret_scalar_get(s1));
        if (s2)
            printf("s2: %lld, ", ferret_scalar_get(s2));
        if (s3)
            printf("s3: %lld, ", ferret_scalar_get(s3));
        if (s4)
            printf("s4: %lld, ", ferret_scalar_get(s4));
        if (s1 || s2 || s3 || s4)
            puts("\n");

        if (h1)
        {
            printf("h1: (U: %u, O: %u, Min: %lld, Max: %lld, S: %lld, C: %u) ",
                   h1->head.underflow, h1->head.overflow, h1->head.val_min, 
                   h1->head.val_max, h1->head.val_sum, h1->head.val_count);
            for (i = 0; i < h1->head.bins; ++i)
            {
                printf("%u, ", h1->data[i]);
            }
            puts("\n");
        }

        told = 0;
        if (l1)
        {
            while (1)
            {
                ret = ferret_list_get(l1, le);
                if (ret == -1)
                    break;
                else if (ret < -1)
                {
                    LOG("l1: Something wrong with list: %d", ret);
                    exit(1);
                }
                printf("l1: time: %lld, diff: %lld\n", le->timestamp,
                       le->timestamp - told);
                told = le->timestamp;
            }
        }

        told = 0;
        if (l2)
        {
            while (1)
            {
                ret = ferret_list_get(l2, le);
                if (ret == -1)
                    break;
                else if (ret < -1)
                {
                    LOG("l2: Something wrong with list: %d", ret);
                    exit(1);
                }
                lec = (ferret_list_entry_common_t *)le;
                printf("l2: time: %lld, %hd.%hd, type: %d, seq.:"
                       " %d, ttime: %lld",
                       lec->timestamp, lec->major, lec->minor,
                       lec->data32[2], lec->data32[3], lec->data64[0]);
                if (lec->data32[2] == 2)  // stop type event
                {
                    printf(", ttdiff: %lld\n", lec->data64[0] - told);
                }
                else
                {
                    told = lec->data64[0];
                    puts("\n");
                }
                // testing unpack stuff here
                {
                    ferret_time_t t1, t2;
                    uint16_t major, minor, instance;
                    uint8_t cpu;
                    uint32_t seq, type;

                    ret = ferret_util_unpack("qhhhbxqll", le,
                                             &t1, &major, &minor, &instance,
                                             &cpu, &t2, &type, &seq);
                    printf("u2: time: %lld, %hd.%hd, type: %d, seq.:"
                           " %d, ttime: %lld\n",
                           t1, major, minor, type, seq, t2);
                }
            }
        }

        if (t1)
        {
            te_count = 0;
            while (1)
            {
                for (i = 0; i < 100; ++i)
                {
                    ret = ferret_tbuf_get(t1, &te[i]);
                    if (ret == -1)
                    {
                        break;
                    }
                    else if (ret < -1)
                    {
                        LOG("t1: Something wrong with trace buffer: %d", ret);
                        exit(1);
                    }
                }

                printf("t1: count: %d\n", i);
                /*
                for (j = 0; j < i; ++j)
                {
                    printf("t1: number: %d, eip %p, tsc %lld, type %hhd\n",
                           te[j].number, (void * )te[j].eip, te[j].tsc,
                           te[j].type);
                }
                */
                if (i != 100)
                    break;
            }
        }

        l4_sleep(1000);
    }

    return 0;
}
