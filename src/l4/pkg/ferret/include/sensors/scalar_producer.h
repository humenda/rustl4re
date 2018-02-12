/**
 * \file   ferret/include/sensors/scalar_producer.h
 * \brief  Scalar producer functions.
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
#ifndef __FERRET_INCLUDE_SENSORS_SCALAR_PRODUCER_H_
#define __FERRET_INCLUDE_SENSORS_SCALAR_PRODUCER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/scalar.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

L4_INLINE void
ferret_scalar_put(ferret_scalar_t * scalar, ferret_time_t data);
L4_INLINE void
ferret_scalar_add(ferret_scalar_t * scalar, ferret_time_t data);
L4_INLINE void
ferret_scalar_sub(ferret_scalar_t * scalar, ferret_time_t data);
L4_INLINE void ferret_scalar_inc(ferret_scalar_t * scalar);
L4_INLINE void ferret_scalar_dec(ferret_scalar_t * scalar);

/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>

// fixme: care for max, min, ...

L4_INLINE void
ferret_scalar_put(ferret_scalar_t * scalar, ferret_time_t data)
{
    scalar->data = data;
}

L4_INLINE void
ferret_scalar_add(ferret_scalar_t * scalar, ferret_time_t data)
{
    ferret_time_t _old, _new;

    do
    {
        _old = scalar->data;
        _new = _old + data;
    } while (! l4util_cmpxchg64((l4_uint64_t*)&(scalar->data), _old, _new));
}

L4_INLINE void
ferret_scalar_sub(ferret_scalar_t * scalar, ferret_time_t data)
{
    ferret_time_t _old, _new;

    do
    {
        _old = scalar->data;
        _new = _old - data;
    } while (! l4util_cmpxchg64((l4_uint64_t*)&(scalar->data), _old, _new));
}

L4_INLINE void ferret_scalar_inc(ferret_scalar_t * scalar)
{
    ferret_time_t _old, _new;

    do
    {
        _old = scalar->data;
        _new = _old + 1;
    } while (! l4util_cmpxchg64((l4_uint64_t*)&(scalar->data), _old, _new));
}

L4_INLINE void ferret_scalar_dec(ferret_scalar_t * scalar)
{
    ferret_time_t _old, _new;

    do
    {
        _old = scalar->data;
        _new = _old - 1;
    } while (! l4util_cmpxchg64((l4_uint64_t*)&(scalar->data), _old, _new));
}

EXTERN_C_END

#endif
