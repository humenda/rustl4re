/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/sys/types.h>
#include <l4/re/env>
#include <l4/libc_backends/clk.h>
#include <l4/rtc/rtc>
#include <l4/crtn/initpriorities.h>

#include <cstdio>

#include "gettime.h"

namespace {

struct Rtc_be
{
  L4rtc::Rtc::Time offset;
  Rtc_be()
  {
    libc_backend_rtc_init();

    int ret;

    offset = 0;
    L4::Cap<L4rtc::Rtc> rtc = L4Re::Env::env()->get_cap<L4rtc::Rtc>("rtc");
    if (!rtc)
      return;

    ret = rtc->get_timer_offset(&offset);

    // error, assume offset 0
    if (ret)
      printf("RTC server not found, assuming 1.1.1970, 0:00 ...\n");
  }
};

static Rtc_be _rtc_be __attribute__((init_priority(INIT_PRIO_RTC_L4LIBC_INIT)));

}

int libc_backend_rt_clock_gettime(struct timespec *tp)
{
  L4rtc::Rtc::Time now = _rtc_be.offset + L4rtc::Rtc::get_timer();
  tp->tv_sec = now / 1000000000;
  tp->tv_nsec = now % 1000000000;
  return 0;
}
