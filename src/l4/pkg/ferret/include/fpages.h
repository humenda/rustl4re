/**
 * \file   ferret/include/fpages.h
 * \brief  IDL wrapper for fpages interface
 *
 * \date   2007-06-21
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_FPAGES_H_
#define __FERRET_INCLUDE_FPAGES_H_

#include <l4/sys/compiler.h>
#include <inttypes.h>

L4_CV int ferret_fpages_request(int ksem, uint16_t major, uint16_t minor,
                                uint16_t instance);

#endif
