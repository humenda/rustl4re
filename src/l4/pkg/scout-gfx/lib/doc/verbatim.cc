/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/scout-gfx/doc/verbatim>
#include <l4/scout-gfx/box_layout>
#include <l4/mag-gfx/clip_guard>

namespace Scout_gfx {
/**************
 ** Verbatim **
 **************/

Verbatim::Verbatim(Color bg)
: bgcol(bg)
{
  set_child_layout(Box_layout::create_vert());
  child_layout()->set_margin(10);
}

void
Verbatim::append(Widget *e)
{
  Parent_widget::append(e);
  child_layout()->add_item(e);
}

void
Verbatim::remove(Widget *e)
{
  Parent_widget::remove(e);
  child_layout()->remove_item(e);
}

void Verbatim::draw(Canvas *c, Point const &p)
{
  static const int pad = 5;
  Rect n = Rect(_size).grow(-pad) + p;

  c->draw_box(n, bgcol);

  Mag_gfx::Clip_guard guard(c, n);
  Parent_widget::draw(c, p);
}

}
