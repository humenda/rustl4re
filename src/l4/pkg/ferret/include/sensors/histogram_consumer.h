/**
 * \file   ferret/include/sensors/histogram_consumer.h
 * \brief  Histogram consumer functions.
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
#ifndef __FERRET_INCLUDE_SENSORS_HISTOGRAM_CONSUMER_H_
#define __FERRET_INCLUDE_SENSORS_HISTOGRAM_CONSUMER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/histogram.h>

EXTERN_C_BEGIN

// fixme: make really inline

L4_INLINE unsigned int
ferret_histo_get(ferret_histo_t * histo, ferret_time_t x);

L4_INLINE ferret_utime_t
ferret_histo_get64(ferret_histo64_t * histo, ferret_time_t x);

void ferret_histo_dump(ferret_histo_t * histo);
void ferret_histo_dump_smart(ferret_histo_t * histo);
void ferret_histo64_dump(ferret_histo64_t * histo);

EXTERN_C_END

#endif
