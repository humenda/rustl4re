/**
 * \file   ferret/include/comm.h
 * \brief  Helper stuff for finding the sensor directory.
 *
 * \date   2007-06-21
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel   <doebel@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_COMM_H_
#define __FERRET_INCLUDE_COMM_H_

#include <l4/re/c/util/cap_alloc.h>

EXTERN_C_BEGIN

l4_cap_idx_t lookup_sensordir(void);

EXTERN_C_END

#endif
