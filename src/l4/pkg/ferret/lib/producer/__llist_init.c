/**
 * \file   ferret/lib/sensor/__llist_init.c
 * \brief  locked list init functions.
 *
 * \date   2007-05-16
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#if FERRET_LLIST_MAGIC != ahK6eeNa
#error Do not directly include this file, use a propper wrapper!
#endif
#undef FERRET_LLIST_MAGIC

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/common.h>

#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

#include <l4/util/bitops.h>

//#include <l4/log/l4log.h>

int PREFIX(init)(PREFIX(t) * list, const char * config, char ** data)
{
    uint32_t size, size_ld, count, count_ld;
    int ret;

    // "<element size>:<element count>"
    ret = sscanf(config, "%u:%u", &size, &count);
    //LOG("Config found: %u:%u", size, count);

    if (ret != 2 ||
        size > FERRET_LLIST_MAX_ELEMENT || size < sizeof(ferret_utime_t) ||
        count > FERRET_LLIST_MAX_COUNT || count < 2)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for element
    size_ld = l4util_log2(size);
    if (size > (1 << size_ld))
        size_ld++;
    size = (1 << size_ld);

    // compute next power of two for count
    count_ld = l4util_log2(count);
    if (count > (1 << count_ld))
        count_ld++;
    count = (1 << count_ld);

    if (((long long)size) * count > SSIZE_MAX)
    {
        //LOG("Warning: buffer would be too large!");
        return -2;
    }

    list->head.value         = 0;
    list->count              = count;
    //list->count_ld           = count_ld;
    list->count_mask         = (1 << count_ld) - 1;
    list->element_size       = size;
    list->element_size_ld    = size_ld;
    list->flags              = 0;

    FERRET_LLIST_INIT(list, data);

    memset(list->data, 0, list->count * list->element_size);

    return 0;
}

// fixme: all of the sensor type functions should be stored in a
//        function table
ssize_t PREFIX(size_config)(const char * config)
{
    uint32_t size, size_ld, count, count_ld;
    int ret;

    // "<element size>:<element count>"
    ret = sscanf(config, "%u:%u", &size, &count);
    //LOG("Config found: %u:%u", size, count);

    if (ret != 2 ||
        size > FERRET_LLIST_MAX_ELEMENT || size < sizeof(ferret_utime_t) ||
        count > FERRET_LLIST_MAX_COUNT || count < 2)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for element
    size_ld = l4util_log2(size);
    if (size > (1 << size_ld))
        size_ld++;
    size = (1 << size_ld);

    // compute next power of two for count
    count_ld = l4util_log2(count);
    if (count > (1 << count_ld))
        count_ld++;
    count = (1 << count_ld);

    if (((long long)size) * count > SSIZE_MAX)
    {
        //LOG("Warning: buffer would be too large!");
        return -2;
    }

    // header + element buffer
    return sizeof(PREFIX(t)) + size * count;
}

ssize_t PREFIX(size)(const PREFIX(t) * list)
{
    uint32_t size, count;

    size  = list->element_size;
    count = list->count;

    return sizeof(PREFIX(t)) + size * count;
}
