/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "init_kip.h"

#include <l4/drivers/of_if.h>

void
init_kip_v2_arch(l4_kernel_info_t* l4i)
{
  L4_drivers::Of_if of_if;
  //l4i->total_ram = of_if.detect_ramsize();
  printf("TBD: set total RAM via mem-descs!\n");
  l4i->frequency_cpu = (l4_uint32_t)of_if.detect_cpu_freq() / 1000; //kHz
  l4i->frequency_bus = (l4_uint32_t)of_if.detect_bus_freq();

  of_if.vesa_set_mode(0x117);
}
