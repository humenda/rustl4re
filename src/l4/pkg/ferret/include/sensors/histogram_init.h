/**
 * \file   ferret/include/sensors/histogram_init.h
 * \brief  Histogram init functions.
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
#ifndef __FERRET_INCLUDE_SENSORS_HISTOGRAM_INIT_H_
#define __FERRET_INCLUDE_SENSORS_HISTOGRAM_INIT_H_

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/histogram.h>
#include <l4/ferret/sensors/common.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

// giving the type here makes sense, as one generic function might be
// able to init several types
int ferret_histo_init(ferret_histo_t * histo, const char * config);
int ferret_histo64_init(ferret_histo64_t * histo, const char * config);

ssize_t ferret_histo_size_config(const char * config);
ssize_t ferret_histo64_size_config(const char * config);

ssize_t ferret_histo_size(const ferret_histo_t * histo);
ssize_t ferret_histo64_size(const ferret_histo64_t * histo);

EXTERN_C_END

#endif
