/**
 * \file
 * \brief Small RTC server test
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/rtc/rtc.h>
#include <l4/util/util.h>
#include <l4/re/env.h>
#include <stdio.h>

int main(void)
{
  l4_uint64_t value;

  l4_cap_idx_t server = l4re_env_get_cap("rtc");
  if (!l4_is_valid_cap(server))
    {
      printf("Error finding 'rtc' cap.\n");
      return 1;
    }

  if (l4rtc_get_offset_to_realtime(server, &value))
    printf("Error: l4rtc_get_offset_to_realtime\n");
  else
    printf("offset-to-realtime: %lld\n", value);

  while (1)
    {
      l4_uint64_t now = l4rtc_get_timer() + value;
      printf("time: %lldns\n", now);
      l4_sleep(400);
    }

  return 0;
}
