/**
 * \file	rtc/include/rtc.h
 * \brief	RTC library interface
 * 
 * \date	06/15/2001
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef L4_RTC_RTC_H
#define L4_RTC_RTC_H

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>
#include <l4/sys/l4int.h>


EXTERN_C_BEGIN

L4_CV int
l4rtc_get_offset_to_realtime(l4_cap_idx_t server, l4_uint64_t *nanoseconds);

L4_CV int
l4rtc_set_offset_to_realtime(l4_cap_idx_t server, l4_uint64_t nanoseconds);

L4_CV l4_uint64_t l4rtc_get_timer(void);

EXTERN_C_END

#endif

