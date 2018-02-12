/*
 * \brief   Just reboot
 * \date    2006-03
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */

/*
 * (c) 2006-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/util/reboot.h>

int main(void)
{
  l4util_reboot();
  return 0;
}
