/**
 * \file   ferret/lib/sensor/scalar_init.c
 * \brief  Scalar init functions.
 *
 * \date   08/11/2005
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
#include <l4/ferret/sensors/scalar.h>
#include <l4/ferret/sensors/scalar_init.h>

#include <stdio.h>
#include <sys/types.h>

//#include <l4/log/l4log.h>

int ferret_scalar_init(ferret_scalar_t * scalar, const char * config)
{
    ferret_time_t low, high;
    int ret;

    // "<low value>:<high value>"
    ret = sscanf(config, "%lld:%lld", &low, &high);
    //LOG("Config found: %lld, %lld", low, high);

    if (ret != 2)
    {
        // LOG("Warning: config string not recognized: %s!", config);
        return -1;
    }
    else
    {
        scalar->low  = low;
        scalar->high = high;
        scalar->data = 0;
    }
    return 0;
}

// fixme: all of the sensor type functions should be stored in a
//        function table
l4_ssize_t ferret_scalar_size_config(const char * config __attribute__((unused)))
{
    return sizeof(ferret_scalar_t);
}

l4_ssize_t ferret_scalar_size(const ferret_scalar_t * scalar __attribute__((unused)))
{
    return sizeof(ferret_scalar_t);
}
