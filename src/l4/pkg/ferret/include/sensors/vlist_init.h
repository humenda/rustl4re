/**
 * \file   ferret/include/sensors/vlist_init.h
 * \brief  Variable list init functions.
 *
 * \date   2007-09-26
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_VLIST_INIT_H_
#define __FERRET_INCLUDE_SENSORS_VLIST_INIT_H_

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/vlist.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

int ferret_vlist_init(ferret_vlist_t * list, const char * config);

ssize_t ferret_vlist_size_config(const char * config);

ssize_t ferret_vlist_size(const ferret_vlist_t * list);

EXTERN_C_END

#endif
