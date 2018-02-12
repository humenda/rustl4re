/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/doc/token>
#include <l4/mag-gfx/canvas>

namespace Scout_gfx {
/***********
 ** Token **
 ***********/

Token::Token(Style *style, const char *str, int len)
: _next(this)
{
  _str               = str;
  _len               = len;
  _style             = style;
  _flags.takes_focus = 0;
  _col               = _style ? _style->color : Color(0, 0, 0);
  _outline           = Color(0, 0, 0, 0);

  if (!_style)
    return
;
  //offset compensates for the outline-offset in draw()
  _size = Area(_style->font->str_w(str, len) + _style->font->str_w(" ", 1),
      _style->font->str_h(str, len)) + Area(2,2);
}

void Token::draw(Canvas *c, Point const &p)
{
  if (!_style)
    return;

  if (_style->attr & Style::ATTR_BOLD)
    _outline = Color(_col.r(), _col.g(), _col.b(), 32);

  Point np = p + Point(1,1);

  if (_outline.a())
    for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++)
      c->draw_string(np + Point(i,j), _style->font, _outline, _str, _len);

  c->draw_string(np, _style->font, _col, _str, _len);

  if (_style->attr & Style::ATTR_UNDERLINE)
    c->draw_box(Rect(_size).bottom(1) + p, Color(0,0,255));
}

}
