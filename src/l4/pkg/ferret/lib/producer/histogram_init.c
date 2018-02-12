/**
 * \file   ferret/lib/sensor/histogram_init.c
 * \brief  Histogram init functions.
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
#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/histogram.h>
#include <l4/ferret/sensors/histogram_init.h>

#include <stdio.h>
#include <sys/types.h>

#include <l4/log/l4log.h>

static void ferret_histo_init_head(ferret_histo_header_t *head, ferret_time_t low,
                                   ferret_time_t high, unsigned int bins);
static void ferret_histo_init_head(ferret_histo_header_t *head, ferret_time_t low,
                                   ferret_time_t high, unsigned int bins)
{
    head->low  = low;
    head->high = high;
    head->bins = bins;
    head->size = high - low;

    head->underflow = 0;
    head->overflow  = 0;
    head->val_min   = FERRET_TIME_MAX;
    head->val_max   = FERRET_TIME_MIN;
    head->val_sum   = 0;
    head->val_count = 0;
}

int ferret_histo_init(ferret_histo_t * histo, const char * config)
{
    ferret_time_t low, high;
    unsigned int bins;
    int ret, i;

    // "<low value>:<high value>:<bins>"
    ret = sscanf(config, "%lld:%lld:%u", &low, &high, &bins);
    //LOG("Config found: %lld, %lld, %u", low, high, bins);

    if (ret != 3 || bins < 1 || low >= high)
    {
        return -1;
        //LOG("Warning: config string not recognized or flawed: %s!", config);
    }
    else
    {
        ferret_histo_init_head(&histo->head, low, high, bins);
        for (i = 0; i < bins; ++i)
            histo->data[i] = 0;
    }
    return 0;
}

int ferret_histo64_init(ferret_histo64_t * histo, const char * config)
{
    ferret_time_t low, high;
    unsigned int bins;
    int ret, i;

    // "<low value>:<high value>:<bins>"
    ret = sscanf(config, "%lld:%lld:%u", &low, &high, &bins);
    //LOG("Config found: %lld, %lld, %u", low, high, bins);

    if (ret != 3 || bins < 1 || low >= high)
    {
        return -1;
        //LOG("Warning: config string not recognized or flawed: %s!", config);
    }
    else
    {
        ferret_histo_init_head(&histo->head, low, high, bins);
        for (i = 0; i < bins; ++i)
            histo->data[i] = 0;
    }
    return 0;
}

// fixme: all of the sensor type functions should be stored in a
//        function table
ssize_t ferret_histo_size_config(const char * config)
{
    ferret_time_t low, high;
    unsigned int bins;
    int ret;
    ferret_histo_t temp;

    ret = sscanf(config, "%lld:%lld:%u", &low, &high, &bins);
    //LOG("Config found: %lld, %lld, %u", low, high, bins);

    if (ret != 3 || bins < 1 || low >= high)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    return sizeof(ferret_histo_t) + sizeof(temp.data[0]) * bins;
}

ssize_t ferret_histo64_size_config(const char * config)
{
    ferret_time_t low, high;
    unsigned int bins;
    int ret;
    ferret_histo64_t temp;

    ret = sscanf(config, "%lld:%lld:%u", &low, &high, &bins);
    //LOG("Config found: %lld, %lld, %u", low, high, bins);

    if (ret != 3 || bins < 1 || low >= high)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    return sizeof(ferret_histo64_t) + sizeof(temp.data[0]) * bins;
}

ssize_t ferret_histo_size(const ferret_histo_t * histo)
{
    return sizeof(ferret_histo_t) + sizeof(histo->data[0]) * histo->head.bins;
}

ssize_t ferret_histo64_size(const ferret_histo64_t * histo)
{
    return sizeof(ferret_histo64_t) + sizeof(histo->data[0]) * histo->head.bins;
}
