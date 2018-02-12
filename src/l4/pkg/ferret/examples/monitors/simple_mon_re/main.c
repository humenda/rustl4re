/**
 * \file   ferret/examples/simple_mon_re/main.c
 * \brief  SimpleMon exampe for L4Re
 *
 * \date   10/03/2009
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel   <doebel@tudos.org>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>
#include <stdio.h>

#include <l4/util/util.h>

#include <l4/ferret/monitor.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/scalar_consumer.h>
//#include <l4/ferret/sensors/histogram_consumer.h>
#include <l4/ferret/sensors/list_consumer.h>
//#include <l4/ferret/sensors/tbuf_consumer.h>

char LOG_tag[9] = "FerSiMo";

int main(void)
{
	ferret_scalar_t *s1, *s2, *s3, *s4;
	ferret_list_moni_t * l1, * l2;
	ferret_list_entry_t * le = malloc(sizeof(ferret_list_entry_t) +
	                                  sizeof(ferret_list_entry_common_t));
	ferret_list_entry_common_t * lec;
	ferret_time_t told;

	int ret;

	l4_cap_idx_t srv = lookup_sensordir();

	l4_sleep(3000);

	// attaching to some sensors

	// scalars ...
	ret = ferret_att(srv, 10, 1, 0, s1);
	if (ret)
	{
		printf("Could not attach to 10:1, ignored\n");
	}
	ret = ferret_att(srv, 10, 2, 0, s2);
    if (ret)
    {
        printf("Could not attach to 10:2, ignored\n");
    }
    ret = ferret_att(srv, 10, 3, 0, s3);
    if (ret)
    {
        printf("Could not attach to 10:3, ignored\n");
    }
    ret = ferret_att(srv, 10, 4, 0, s4);
    if (ret)
    {
        printf("Could not attach to 10:4, ignored\n");
    }

    // lists ...
    ret = ferret_att(srv, 12, 1, 0, l1);
    if (ret)
    {
        printf("Could not attach to 12:1, ignored\n");
    }

    ret = ferret_att(srv, 12, 2, 0, l2);
    if (ret)
    {
        printf("Could not attach to 12:2, ignored\n");
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
                    printf("l1: Something wrong with list: %d\n", ret);
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
                    printf("l2: Something wrong with list: %d\n", ret);
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


		l4_sleep(1000);
	}

    return 0;
}
