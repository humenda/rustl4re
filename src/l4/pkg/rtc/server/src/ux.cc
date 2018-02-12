/**
 * \file    rtc/server/src/ux.c
 * \brief   Get current time
 *
 * \date    09/26/2003
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/lxfuxlibc/lxfuxlc.h>

#include <l4/sys/thread.h>
#include <l4/re/env.h>
#include <l4/util/rdtsc.h>
#include <l4/util/kip.h>

#include <time.h>
#include <stdio.h>

#include "rtc.h"

static void printit(void)
{
  time_t t = lx_time(NULL);
  struct tm *r;

  r = gmtime(&t);

  printf("Date:%02d.%02d.%04d Time:%02d:%02d:%02d\n",
         r->tm_mday, r->tm_mon + 1, r->tm_year + 1900,
         r->tm_hour, r->tm_min, r->tm_sec);
}

struct Ux_rtc : Rtc
{
  int get_time(l4_uint64_t *offset)
  {
    l4_uint64_t current_ns = l4_tsc_to_ns(l4_rdtsc());
    l4_uint64_t ti = lx_time(NULL);
    *offset = ti * 1000000000 - current_ns;

    printit();

    return 0;
  }

  int set_time(l4_uint64_t)
  { return 0; }

  bool probe()
  {
    printf("probe UX RTC\n");
    if (l4util_kip_kernel_is_ux(l4re_kip()))
      {
        l4_thread_control_start();
        l4_thread_control_ux_host_syscall(1);
        l4_thread_control_commit(l4re_env()->main_thread);
        return true;
      }

    return false;
  }
};

static Ux_rtc __ux_rtc;
