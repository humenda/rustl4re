/**
 * \file   ferret/lib/sensors/common.c
 * \brief  Common sensor header functions
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

#include <l4/ferret/sensors/common.h>

void ferret_common_init(ferret_common_t * c,
                        uint16_t major, uint16_t minor,
                        uint16_t instance, uint16_t type,
                        l4dm_dataspace_t ds)
{
    c->major    = major;
    c->minor    = minor;
    c->instance = instance;
    c->type     = type;
    c->ds       = ds;
}
