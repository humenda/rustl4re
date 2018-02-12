/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/layout>
#include <cstdio>

using namespace Mag_gfx;

namespace Scout_gfx {

void
Layout::remove_item(Layout_item *i)
{
  Layout_item *c;
  for (int x = 0; (c = item(x)); ++x)
    if (c == i)
      {
        remove_item(x);
	invalidate();
	return;
      }
}



}
