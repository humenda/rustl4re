/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/mag-gfx/canvas>
#include <l4/mag-gfx/clip_guard>

#include "view"

namespace Mag_server {

using namespace Mag_gfx;

class Background : public View
{
public:
  explicit Background(Area const &size) : View(Rect(Point(0,0), size)) {}
  void draw(Canvas *c, View_stack const *, Mode) const
  {
    Clip_guard g(c, *this);
    c->draw_box(*this, Rgb32::Color(25, 37, 50));
  }

  void handle_event(Hid_report *, Point const &, bool) {}

};

}
