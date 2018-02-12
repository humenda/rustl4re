/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#include <l4/util/port_io.h>
#include <l4/util/cpu.h>

void
reboot_arch(void) __attribute__((noreturn));

void
reboot_arch(void)
{
  while (l4util_in8(0x64) & 0x02)
    l4util_iodelay();
  l4util_out8(0x60, 0x64);
  l4util_iodelay();

  while (l4util_in8(0x64) & 0x02)
    l4util_iodelay();
  l4util_out8(0x04, 0x60);
  l4util_iodelay();

  while (l4util_in8(0x64) & 0x02)
    l4util_iodelay();
  l4util_out8(0xFE, 0x64);
  l4util_iodelay();

  for (;;)
    l4util_cpu_pause();
}
