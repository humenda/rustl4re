/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/loadbar>
#include <l4/mag-gfx/clip_guard>


#include <cstdio>

#define LOADBAR_RGBA _binary_loadbar_rgba_start
#define REDBAR_RGBA _binary_redbar_rgba_start
#define WHITEBAR_RGBA    _binary_whitebar_rgba_start

extern unsigned char LOADBAR_RGBA[];
extern unsigned char REDBAR_RGBA[];
extern unsigned char WHITEBAR_RGBA[];

namespace Scout_gfx {

Loadbar::Loadbar(Factory *f, Loadbar_listener *listener, Mag_gfx::Font *font)
: _active(listener ? true : false),
  _ev_handler(this, listener),
  _value(0), _max_value(100),
  _txt(""), _txt_len(0), _txt_sz(Area(0,0)),
  _font(font)
{
  _size = Area(0,  _LH);
  _cover = f->create_icon();
  _cover->rgba(LOADBAR_RGBA, Area(16, 16));
  _cover->alpha(100);
  _cover->focus_alpha(150);

  _bar = f->create_icon();
  _bar->rgba(_active ? REDBAR_RGBA : LOADBAR_RGBA, Area(16, 16));
  _bar->alpha(_active ? 150 : 255);
  _bar->default_alpha(150);

  if (_active)
    {
      event_handler(&_ev_handler);
      _bar->event_handler(&_ev_handler);
      _cover->event_handler(&_ev_handler);
    }

  append(_cover);
  append(_bar);
  _cover->set_geometry(Rect(geometry().area()));
  _bar->set_geometry(Rect(geometry().area()));
}


void
Loadbar::_update_bar_geometry(int w)
{
  int max_w = w - _LW;
  int bar_w = _max_value ? (_value * max_w) / _max_value : max_w;
  bar_w += _LW;
  _bar->set_geometry(Rect(_bar->pos(), Area(bar_w, geometry().h())));
}


void
Loadbar::value(int value)
{
  _value = std::max(std::min(value, _max_value), 0);
  _update_bar_geometry(_size.w());
}


void
Loadbar::max_value(int max_value)
{
  _max_value = max_value;
  _update_bar_geometry(_size.w());
}

void
Loadbar::txt(const char *txt)
{
  if (!_font)
    return;

  _txt     = txt;
  _txt_sz  = _font->str_sz(_txt);
  _txt_len = strlen(_txt);
  invalidate();
}

/**
 * Element interface
 */
void
Loadbar::set_geometry(Rect const &r)
{
  using Mag_gfx::Point;
  using Mag_gfx::Area;
  using Mag_gfx::Rect;

  Parent_widget::set_geometry(r);
  _cover->set_geometry(Rect(r.area()));
  _update_bar_geometry(r.w());
}

void
Loadbar::draw(Mag_gfx::Canvas *c, Mag_gfx::Point const &p)
{
  using Mag_gfx::Point;
  Parent_widget::draw(c, p);

  if (!_font)
    return;

  Rect pos(geometry().area());
  Point txt = p + pos.center(_txt_sz).max(pos.p1() + Point(8, 0)) - Point(0, 1);

  /* shrink clipping area to text area (limit too long label) */
    {
      Mag_gfx::Clip_guard g(c, c->clip() & (pos + p));

      c->draw_string(txt + Point(0,1), _font, Color(0,0,0,150), _txt);
      c->draw_string(txt, _font, Color(255,255,255,230), _txt);
    }

}

void
Loadbar::mfocus(int flag)
{
  if (!_active)
    return;

  _bar->mfocus(flag);
  _cover->mfocus(flag);
}



void
Kbyte_loadbar::_print_kbytes(int kbytes, char *dst, int dst_len)
{
  if (kbytes > 10 * 1024 * 1024)
    snprintf(dst, dst_len, "%d GByte", kbytes / 1024 / 1024);
  else if (kbytes >= 10 * 1024)
    snprintf(dst, dst_len, "%d MByte", kbytes / 1024);
  else
    snprintf(dst, dst_len, "%d KByte", kbytes);
}

void
Kbyte_loadbar::_update_label()
{
  char value_buf[12];
  char max_buf[12];

  _print_kbytes(Loadbar::value(), value_buf, sizeof(value_buf));
  _print_kbytes(Loadbar::max_value(), max_buf, sizeof(max_buf));

  snprintf(_label, sizeof(_label), "%s / %s", value_buf, max_buf);

  Loadbar::txt(_label);
}


Kbyte_loadbar::Kbyte_loadbar(Factory *f, Loadbar_listener *listener,
                             Mag_gfx::Font *font)
: Loadbar(f, listener, font)
{
  _label[0] = 0;
  _update_label();
}

void
Kbyte_loadbar::value(int val)
{
  Loadbar::value(val);
  _update_label();
}

void
Kbyte_loadbar::max_value(int max_value)
{
  Loadbar::max_value(max_value);
  _update_label();
}

}
