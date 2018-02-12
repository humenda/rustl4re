/**
 * \file   ferret/lib/sensor/scalar_consumer.c
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
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/scalar.h>
#include <l4/ferret/sensors/scalar_consumer.h>

ferret_time_t ferret_scalar_get(ferret_scalar_t * scalar)
{
    return scalar->data;
}
