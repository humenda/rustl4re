/**
 * \file   ferret/include/sensors/list_init.h
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
#ifndef __FERRET_INCLUDE_SENSORS_LIST_INIT_H_
#define __FERRET_INCLUDE_SENSORS_LIST_INIT_H_

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>
#include <unistd.h>

EXTERN_C_BEGIN

// giving the type here makes sense, as one generic function might be
// able to init several types
int ferret_list_init(ferret_list_t * list, const char * config);

ssize_t ferret_list_size_config(const char * config);

ssize_t ferret_list_size(const ferret_list_t * list);

EXTERN_C_END

#endif
