/**
 * \file   ferret/include/sensors/scalar_consumer.h
 * \brief  Scalar consumer functions.
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
#ifndef __FERRET_INCLUDE_SENSORS_SCALAR_CONSUMER_H_
#define __FERRET_INCLUDE_SENSORS_SCALAR_CONSUMER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/scalar.h>

EXTERN_C_BEGIN

ferret_time_t ferret_scalar_get(ferret_scalar_t * scalar);

EXTERN_C_END

#endif
