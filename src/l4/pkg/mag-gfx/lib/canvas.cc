/*
 * (c) 2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/mag-gfx/canvas>

namespace Mag_gfx {

void
Canvas::draw_rect(Rect const &r, Rgba32::Color color, int width)
{
  Rect_tuple t;
  if (width > 0)
    t = r - r.offset(width, width, -width, -width);
  else
    t = r.offset(width, width, -width, -width) - r;

  draw_box(t[0], color);
  draw_box(t[1], color);
  draw_box(t[2], color);
  draw_box(t[3], color);
}

}
