/*!
 * \file   keymap.c
 * \brief  convert keyboard value to ascii value
 *
 * \date   Dec 2007
 * \author Adam Lackorznynski <adam@os.inf.tu-dresden.de>
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

#include <l4/l4con/l4con_ev.h>
#include "keymap.h"

int l4con_map_keyinput_to_ascii(unsigned value, unsigned shift)
{
  if (value < 128 && shift < 2)
    return keymap[value][shift];

  return 0;
}
