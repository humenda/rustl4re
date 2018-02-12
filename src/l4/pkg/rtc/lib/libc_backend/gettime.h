/*!
 * \file   rtc/lib/libc_backend/time/gettime.h
 * \brief  architecture specific handling
 *
 * \date   2007-11-23
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2007-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __RTC__LIB__LIBC_BACKEND__TIME__GETTIME_H__
#define __RTC__LIB__LIBC_BACKEND__TIME__GETTIME_H__

#include <l4/re/env.h>
#include <l4/sys/l4int.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/rdtsc.h>

static inline void libc_backend_rtc_init(void)
{
  l4_calibrate_tsc(l4re_kip());
}

static inline void libc_backend_rtc_get_s_and_ns(l4_uint32_t *s, l4_uint32_t *ns)
{
  l4_tsc_to_s_and_ns(l4_rdtsc(), s, ns);
}

#else

static inline void libc_backend_rtc_init(void)
{
}

static inline void libc_backend_rtc_get_s_and_ns(l4_uint32_t *s, l4_uint32_t *ns)
{
  l4_kernel_clock_t c = l4_kip_clock(l4re_kip());

  *s = c / 1000000;
  *ns = (c % 1000000) * 1000;
}

#endif

#endif /* ! __RTC__LIB__LIBC_BACKEND__TIME__GETTIME_H__ */
