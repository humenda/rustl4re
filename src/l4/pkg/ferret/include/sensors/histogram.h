/**
 * \file   ferret/include/sensors/histogram.h
 * \brief  Functions to operate on histograms.
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
#ifndef __FERRET_INCLUDE_SENSORS_HISTOGRAM_H_
#define __FERRET_INCLUDE_SENSORS_HISTOGRAM_H_

#include <l4/ferret/sensors/common.h>
#include <l4/ferret/types.h>

typedef struct
{
    ferret_common_t header;

    ferret_time_t   low;        // lower bound for histogram data (inclusive)
    ferret_time_t   high;       // upper bound for histogram data (exclusive)

    ferret_time_t   size;       // precomputed multiplier (high - low)
    unsigned int    bins;       // number of bins in histogram
    unsigned int    underflow;  // number of underflows (below low) occurred
    unsigned int    overflow;   // number of overflows (above high) occurred

    ferret_time_t   val_min;    // minimum value put in so far
    ferret_time_t   val_max;    // maximum value put in so far
    ferret_time_t   val_sum;    // sum of the weighted values in the histogram
    unsigned int    val_count;  // number of values in the histogram
} ferret_histo_header_t;

typedef struct
{
    ferret_histo_header_t head;       // header
    unsigned int          data[0];    // data points
}  ferret_histo_t;

typedef struct
{
    ferret_histo_header_t head;       // header
    ferret_utime_t        data[0];    // data points
} ferret_histo64_t;

#endif
