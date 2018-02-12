/**
 * \file   ferret/include/gcc_instrument.h
 * \brief  Support functions for gcc-function-level instrumentation
 *
 * \date   2007-10-09
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_GCC_INSTRUMENT_H
#define __FERRET_INCLUDE_GCC_INSTRUMENT_H

#include <l4/sys/types.h>
#include <stddef.h>

/* Set function pointer for get_threadid-function and create sensor.
 */
L4_CV void ferret_gcc_instrument_init(void *(*alloc)(size_t s),
                                      l4_cap_idx_t (*myself)(void))
    __attribute__ ((no_instrument_function));

#endif
