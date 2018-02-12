/**
 * \file   ferret/include/sensors/histogram_producer.h
 * \brief  Histogram producer functions.
 *
 * \date   15/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_HISTOGRAM_PRODUCER_H_
#define __FERRET_INCLUDE_SENSORS_HISTOGRAM_PRODUCER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/histogram.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

// basic histogram functions
L4_INLINE void ferret_histo_inc(ferret_histo_t * histo, ferret_time_t x);
L4_INLINE void ferret_histo64_inc(ferret_histo64_t * histo, ferret_time_t x);
L4_INLINE void ferret_histo_add(ferret_histo_t * histo, ferret_time_t x,
                                unsigned int val);
L4_INLINE void ferret_histo64_add(ferret_histo64_t * histo, ferret_time_t x,
                                  ferret_utime_t val);

// direct bin manipulation, i.e., use the histogram as a array
L4_INLINE void ferret_histo_bin_inc(ferret_histo_t * histo, int bin);
L4_INLINE void ferret_histo64_bin_inc(ferret_histo64_t * histo, int bin);
L4_INLINE void ferret_histo_bin_add(ferret_histo_t * histo, int bin,
                                    unsigned int val);
L4_INLINE void ferret_histo64_bin_add(ferret_histo64_t * histo, int bin,
                                      ferret_utime_t val);
L4_INLINE void ferret_histo_bin_set(ferret_histo_t * histo, int bin,
                                    unsigned int val);
L4_INLINE void ferret_histo64_bin_set(ferret_histo64_t * histo, int bin,
                                      ferret_utime_t val);

/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>


L4_INLINE void ferret_histo_inc(ferret_histo_t * histo, ferret_time_t x)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;

    // use atomic data access, such that it can be used from several places
    if (L4_UNLIKELY(bin < 0))
        l4util_inc32(&histo->head.underflow);
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        l4util_inc32(&histo->head.overflow);
    else
        l4util_inc32(&(histo->data[bin]));

    if (L4_UNLIKELY(x < histo->head.val_min))
        histo->head.val_min = x;     // fixme: make atomic
    if (L4_UNLIKELY(x > histo->head.val_max))
        histo->head.val_max = x;     // fixme: make atomic
    histo->head.val_sum += x;        // fixme: make atomic
    l4util_inc32(&histo->head.val_count);
}

L4_INLINE void ferret_histo64_inc(ferret_histo64_t * histo, ferret_time_t x)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;

    // FIXME: use atomic data access, such that it can be used from several places
    //      --> will work only for 64bit machines
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow++;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow++;
    else
        histo->data[bin]++;

    if (L4_UNLIKELY(x < histo->head.val_min))
        histo->head.val_min = x;     // fixme: make atomic
    if (L4_UNLIKELY(x > histo->head.val_max))
        histo->head.val_max = x;     // fixme: make atomic
    histo->head.val_sum += x;        // fixme: make atomic
    histo->head.val_count++;
}

L4_INLINE void ferret_histo_add(ferret_histo_t * histo,
                                ferret_time_t x, unsigned int val)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;

    // use atomic data access, such that it can be used from several places
    if (L4_UNLIKELY(bin < 0))
        l4util_add32(&histo->head.underflow, val);
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        l4util_add32(&histo->head.overflow, val);
    else
        l4util_add32(&(histo->data[bin]), val);

    if (L4_UNLIKELY(x < histo->head.val_min))
        histo->head.val_min = x;     // fixme: make atomic
    if (L4_UNLIKELY(x > histo->head.val_max))
        histo->head.val_max = x;     // fixme: make atomic
    histo->head.val_sum += val * x;  // fixme: make atomic
    l4util_inc32(&histo->head.val_count);
}

L4_INLINE void ferret_histo64_add(ferret_histo64_t * histo,
                                  ferret_time_t x, ferret_utime_t val)
{
    int bin;

    bin = (x - histo->head.low) * histo->head.bins / histo->head.size;

    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow += val;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow += val;
    else
        histo->data[bin] += val;

    if (L4_UNLIKELY(x < histo->head.val_min))
        histo->head.val_min = x;     // fixme: make atomic
    if (L4_UNLIKELY(x > histo->head.val_max))
        histo->head.val_max = x;     // fixme: make atomic
    histo->head.val_sum += val * x;  // fixme: make atomic
    histo->head.val_count++;
}

L4_INLINE void ferret_histo_bin_inc(ferret_histo_t * histo, int bin)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        l4util_inc32(&histo->head.underflow);
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        l4util_inc32(&histo->head.overflow);
    else
        l4util_inc32(&(histo->data[bin]));
}

L4_INLINE void ferret_histo64_bin_inc(ferret_histo64_t * histo, int bin)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow++;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow++;
    else
        histo->data[bin]++;
}

L4_INLINE void ferret_histo_bin_add(ferret_histo_t * histo, int bin,
                                    unsigned int val)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        l4util_add32(&histo->head.underflow, val);
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        l4util_add32(&histo->head.overflow, val);
    else
        l4util_add32(&(histo->data[bin]), val);
}

L4_INLINE void ferret_histo64_bin_add(ferret_histo64_t * histo, int bin,
                                      ferret_utime_t val)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow += val;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow += val;
    else
        histo->data[bin] += val;
}

L4_INLINE void ferret_histo_bin_set(ferret_histo_t * histo, int bin,
                                    unsigned int val)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow = val;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow = val;
    else
        histo->data[bin] = val;
}

L4_INLINE void ferret_histo64_bin_set(ferret_histo64_t * histo, int bin,
                                      ferret_utime_t val)
{
    // FIXME: use atomic data access, such that it can be used from
    //        several places
    if (L4_UNLIKELY(bin < 0))
        histo->head.underflow = val;
    else if (L4_UNLIKELY(bin >= histo->head.bins))
        histo->head.overflow = val;
    else
        histo->data[bin] = val;
}

EXTERN_C_END

#endif
