/**
 * \file   ferret/include/sensors/common.h
 * \brief  Common sensor function and types
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
#ifndef __FERRET_INCLUDE_SENSORS_COMMON_H_
#define __FERRET_INCLUDE_SENSORS_COMMON_H_

#include <l4/ferret/types.h>

#include <l4/sys/compiler.h>

typedef struct
{
    uint16_t         major;
    uint16_t         minor;
    uint16_t         instance;
    uint16_t         type;
	l4_cap_idx_t     ds_cap;
	uint32_t         ds_size;
}  ferret_common_t;


EXTERN_C_BEGIN

void ferret_common_init(ferret_common_t * c,
                        uint16_t major, uint16_t minor,
                        uint16_t instance, uint16_t type,
                        l4re_ds_t *dataspace);

EXTERN_C_END

#endif
