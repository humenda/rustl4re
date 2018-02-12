/**
 * \file   ferret/include/sensors/scalar_init.h
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
#ifndef __FERRET_INCLUDE_SENSORS_SCALAR_INIT_H_
#define __FERRET_INCLUDE_SENSORS_SCALAR_INIT_H_

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/scalar.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

// giving the type here makes sense, as one generic function might be
// able to init several types
int ferret_scalar_init(ferret_scalar_t * scalar, const char * config);

l4_ssize_t ferret_scalar_size_config(const char * config);

l4_ssize_t ferret_scalar_size(const ferret_scalar_t * scalar);

EXTERN_C_END

#endif
