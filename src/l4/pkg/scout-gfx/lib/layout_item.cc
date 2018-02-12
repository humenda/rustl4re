/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/layout_item>

namespace Scout_gfx {

using namespace Mag_gfx;

Rect
Layout_item::aligned_rect(Rect const &r)
{
  Alignment a = alignment();
  if (!a)
    return r;

  Area max = max_size();
  Area s = preferred_size();
  Orientations exp = expanding();

  if ((exp & Horizontal) || !(a & Align_horizontal_m))
    s.w(std::min(r.w(), max.w()));

  if ((exp & Vertical) || !(a & Align_vertical_m))
    s.h(std::min(r.h(), max.h()));
  else if (has_height_for_width())
    s.h(std::min(std::min(s.h(), height_for_width(s.w())), max.h()));

  s = s.min(r.area());

  Point mv = r.p1();

  if (a & Align_bottom)
    mv += Point(0, r.h() - s.h());
  else if (!(a & Align_top))
    mv += Point(0, (r.h() - s.h()) / 2);

  if (a & Align_right)
    mv += Point(r.w() - s.w(), 0);
  else if (!(a & Align_left))
    mv += Point((r.w() - s.w()) / 2, 0);

  return Rect(mv, s);
}

Area
Layout_item::aligned_max_size() const
{
  Area s = max_size();
  Alignment a = alignment();

  if (a & Align_horizontal_m)
    s.w(Area::Max_w);

  if (a & Align_vertical_m)
    s.h(Area::Max_h);

  return s;
}

void
Layout_item::aligned_set_geometry(Rect const &g)
{
  set_geometry(aligned_rect(g));
}

}
