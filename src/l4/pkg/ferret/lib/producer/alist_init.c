/**
 * \file   ferret/lib/sensor/alist_init.c
 * \brief  Atomic list init functions.
 *
 * \date   2007-07-23
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/ferret/sensors/alist.h>
#include <l4/ferret/sensors/alist_init.h>

#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

#include <l4/util/bitops.h>

int ferret_alist_init(ferret_alist_t * list, const char * config)
{
    uint32_t count, count_ld;
    int ret;

    // "<element count>"
    ret = sscanf(config, "%u", &count);

    if (ret != 1 ||
        count > FERRET_ALIST_MAX_COUNT || count < 2)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for count
    count_ld = l4util_log2(count);
    if (count > (1 << count_ld))
        count_ld++;
    count = (1 << count_ld);

    if (((long long)FERRET_ALIST_ELEMENT_SIZE) * count > SSIZE_MAX)
    {
        //LOG("Warning: buffer would be too large!");
        return -2;
    }

    list->head               = 0;
    list->count              = count;
    list->count_ld           = count_ld;
    list->count_mask         = (1 << count_ld) - 1;
    list->flags              = 0;

    memset(list->data, 0, list->count * FERRET_ALIST_ELEMENT_SIZE);

    return 0;
}

ssize_t ferret_alist_size_config(const char * config)
{
    uint32_t count, count_ld;
    int ret;

    // "<element count>"
    ret = sscanf(config, "%u", &count);

    if (ret != 1 ||
        count > FERRET_ALIST_MAX_COUNT || count < 2)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for count
    count_ld = l4util_log2(count);
    if (count > (1 << count_ld))
        count_ld++;
    count = (1 << count_ld);

    if (((long long)FERRET_ALIST_ELEMENT_SIZE) * count > SSIZE_MAX)
    {
        //LOG("Warning: buffer would be too large!");
        return -2;
    }

    // header + element buffer
    return sizeof(ferret_alist_t) + count * FERRET_ALIST_ELEMENT_SIZE;
}

ssize_t ferret_alist_size(const ferret_alist_t * list)
{
    return sizeof(ferret_alist_t) + FERRET_ALIST_ELEMENT_SIZE * list->count;
}
