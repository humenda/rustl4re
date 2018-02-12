/**
 * \file   ferret/lib/sensor/list_init.c
 * \brief  List init functions.
 *
 * \date   23/11/2005
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
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/list_init.h>

#include <stdio.h>
#include <sys/types.h>

int ferret_list_init(ferret_list_t * list, const char * config)
{
    uint32_t size, count, i;
    int ret;
    ferret_list_index_t * index;

    // "<element size>:<element count>"
    ret = sscanf(config, "%u:%u", &size, &count);
    //printf("Config found: %u:%u\n", size, count);

    if (ret != 2 ||
        size > 4096 || size < sizeof(ferret_utime_t) ||
        count > (2 << FERRET_INDEX_BITS) - 2 || count < 2)
    {
        printf("Warning: config string not recognized or flawed: %s!\n", config);
        return -1;
    }

    // align element size to 8 bytes
    size += 7;
    size -= size % 8;

    list->head.parts.element = count;
    list->head.parts.index   = 0;
    list->tail.value         = 0;
    list->count              = count;
    list->element_size       = size;
    list->flags              = 0;
    list->element_offset     = list->count * sizeof(ferret_list_index_t);
	list->element_offset    += sizeof(ferret_list_t);
    // care for cache line size alignment
    list->element_offset += FERRET_ASSUMED_CACHE_LINE_SIZE - 1;
    list->element_offset -= list->element_offset %
                            FERRET_ASSUMED_CACHE_LINE_SIZE;

    index = (ferret_list_index_t *)(list->data);
	//printf("list @ %p, data %p\n", list, list->data);

    for (i = 0; i < count; ++i)
    {
        index[i].parts.element = 0;
        index[i].parts.index   = i;
    }

    return 0;
}

// fixme: all of the sensor type functions should be stored in a
//        function table
ssize_t ferret_list_size_config(const char * config)
{
    uint32_t size, count;
    int ret;
    uint32_t ret_size;

    // "<element size>:<element count>"
    ret = sscanf(config, "%u:%u", &size, &count);
    //LOG("Config found: %u:%u", size, count);

    if (ret != 2 ||
        size > 4096 || size < sizeof(ferret_utime_t) ||
        count > (2 << FERRET_INDEX_BITS) - 2 || count < 2)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // align element size to 8 bytes
    size += 7;
    size -= size % 8;

    // header + index buffer + ...
    ret_size = sizeof(ferret_list_t) +
               sizeof(ferret_list_index_t) * count;
    // ... now care for alignment of elements
    ret_size += FERRET_ASSUMED_CACHE_LINE_SIZE - 1;
    ret_size -= ret_size % FERRET_ASSUMED_CACHE_LINE_SIZE;

    // ... element buffer
    ret_size += size * count;

    return ret_size;
}

ssize_t ferret_list_size(const ferret_list_t * list)
{
    uint32_t size, count;

    size  = list->element_size;
    count = list->count;

    return sizeof(ferret_list_t) +
           size * count +
           sizeof(ferret_list_index_t) * count;
}
