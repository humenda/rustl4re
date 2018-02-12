/**
 * \file   ferret/lib/sensor/vlist_init.c
 * \brief  Variable list init functions.
 *
 * \date   2007-09-27
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/ferret/sensors/vlist.h>
#include <l4/ferret/sensors/vlist_init.h>

#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

#include <l4/util/bitops.h>

int ferret_vlist_init(ferret_alist_t * list, const char * config)
{
    uint32_t size, size_ld;
    int ret;

    // "<sensor size>"
    ret = sscanf(config, "%u", &count);

    if (ret != 1 || size > FERRET_VLIST_MAX_SIZE || size < 4096)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for size
    size_ld = l4util_log2(size);
    if (size > (1 << size_ld))
        size_ld++;
    size = (1 << size_ld);

    list->head       = 0;
    list->tail.value = 0;
    list->size       = size;
    list->size_ld    = size_ld;
    list->size_mask  = (1 << size_ld) - 1;
    list->flags      = 0;

    memset(list->data, 0, list->size);

    return 0;
}

ssize_t ferret_alist_size_config(const char * config)
{
    uint32_t size, size_ld;
    int ret;

    // "<sensor size>"
    ret = sscanf(config, "%u", &count);

    if (ret != 1 || size > FERRET_VLIST_MAX_SIZE || size < 4096)
    {
        //LOG("Warning: config string not recognized or flawed: %s!", config);
        return -1;
    }

    // compute next power of two for size
    size_ld = l4util_log2(size);
    if (size > (1 << size_ld))
        size_ld++;
    size = (1 << size_ld);

    // header + element buffer
    return sizeof(ferret_vlist_t) + size;
}

ssize_t ferret_alist_size(const ferret_alist_t * list)
{
    return sizeof(ferret_vlist_t) + size;
}
