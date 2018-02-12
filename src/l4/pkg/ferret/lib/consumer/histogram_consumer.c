/**
 * \file   ferret/lib/sensor/histogram_consumer.c
 * \brief  Histogram consumer functions.
 *
 * \date   17/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/histogram.h>
#include <l4/ferret/sensors/histogram_consumer.h>

#include <stdio.h>

L4_INLINE unsigned int
ferret_histo_get(ferret_histo_t * histo, ferret_time_t x)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;
    if (bin < 0 || bin >= histo->head.bins)
        return 0;  // fail fast an silent, caller should know our range
    else
        return histo->data[bin];
}

L4_INLINE ferret_utime_t
ferret_histo_get64(ferret_histo64_t * histo, ferret_time_t x)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;
    if (bin < 0 || bin >= histo->head.bins)
        return 0;  // fail fast an silent, caller should know our range
    else
        return histo->data[bin];
}

static void ferret_histo_dump_head(ferret_histo_header_t * h)
{
    printf("Histogram(%hu:%hu:%hu:%hu) [%lld, %lld>, bins: %u,"
           " underflow: %u, overflow: %u\n",
           h->header.major, h->header.minor,
           h->header.instance, h->header.type,
           h->low, h->high, h->bins, h->underflow, h->overflow);
}

void ferret_histo_dump(ferret_histo_t * h)
{
    unsigned int i;

    ferret_histo_dump_head(&h->head);
    for (i = 0; i < h->head.bins; i++)
    {
        printf("%u\t%u\n", i, h->data[i]);
    }

}

void ferret_histo_dump_smart(ferret_histo_t * h)  // what a name :-/
{
    unsigned int i;

    ferret_histo_dump_head(&h->head);
    // do not print consecutive lines of 0s, only the border ones
    if (h->head.bins < 3)  // print everything very small histograms
    {
        for (i = 0; i < h->head.bins; i++)
        {
            printf("%u\t%u\n", i, h->data[i]);
        }
    }
    else
    {
        for (i = 0; i < h->head.bins; i++)
        {
            if ((h->data[i] == 0)                                  &&
                ((i == 0)                || (h->data[i - 1] == 0)) &&
                ((i == h->head.bins - 1) || (h->data[i + 1] == 0)))
            {
                continue;  // skip if we have at least three 0s in a row
            }
            printf("%u\t%u\n", i, h->data[i]);
        }
    }
}

void ferret_histo64_dump(ferret_histo64_t * h)
{
    unsigned int i;

    ferret_histo_dump_head(&h->head);
    for (i = 0; i < h->head.bins; i++)
    {
        printf("%u: %llu\n", i, h->data[i]);
    }
}
