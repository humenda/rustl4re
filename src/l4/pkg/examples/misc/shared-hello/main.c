/*
 * \brief   A 'hello' program linked against shared libraries.
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * This program is built with shared libraries.
 */

/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <unistd.h>

int main(void)
{
  while (1)
    {
      puts("Hi World! (shared)");
      sleep(1);
    }
  return 0;
}
