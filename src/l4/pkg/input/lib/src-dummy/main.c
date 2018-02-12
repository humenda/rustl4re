/*
 * Dummy inputlib with no functionality.
 *
 * by Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 */
/*
 * (c) 2004-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/input/libinput.h>
#include <l4/sys/err.h>

L4_CV int l4input_ispending(void)
{
  return 0;
}

L4_CV int l4input_flush(void *buffer, int count)
{
  (void)buffer; (void)count;
  return 0;
}

L4_CV int l4input_pcspkr(int tone)
{
  (void)tone;
  return -L4_ENODEV;
}

L4_CV int l4input_init(int prio, L4_CV void (*handler)(struct l4input *))
{
  (void)prio; (void)handler;
  return 0;
}
