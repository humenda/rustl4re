/**
 * \file   ferret/include/sensors/alist_init.h
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
#ifndef __FERRET_INCLUDE_SENSORS_ALIST_INIT_H_
#define __FERRET_INCLUDE_SENSORS_ALIST_INIT_H_

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/alist.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

int ferret_alist_init(ferret_alist_t * list, const char * config);

ssize_t ferret_alist_size_config(const char * config);

ssize_t ferret_alist_size(const ferret_alist_t * list);

EXTERN_C_END

#endif
