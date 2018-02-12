/*
 * \brief   Document navigation element
 * \date    2005-11-23
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */
/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <l4/scout-gfx/doc/navbar>
#include <l4/scout-gfx/icon>
#include <l4/scout-gfx/factory>
#include <l4/scout-gfx/box_layout>
//#include "styles.h"
//#include "browser.h"

#include <l4/mag-gfx/clip_guard>

#define NAV_NEXT_RGBA _binary_nav_next_rgba_start
#define NAV_PREV_RGBA _binary_nav_prev_rgba_start

extern unsigned char NAV_NEXT_RGBA[];
extern unsigned char NAV_PREV_RGBA[];

using namespace Scout_gfx;
/**
 * Configuration
 */
enum {
	ARROW_H = 64,   /* height of arrow gfx */
	ARROW_W = 64,   /* width  of arrow gfx */
};


Navbar::Navbar(Factory *f, Style const *style)
: _next_anchor(this), _prev_anchor(this), _style(*style)
{
  _next_title  = _prev_title  = 0;
  set_child_layout(Box_layout::create_horz());
  child_layout()->set_alignment(Mag_gfx::Align_bottom);

  _prev_icon = f->create_icon(NAV_PREV_RGBA, Area(64, 64));
  _prev_icon->alpha(100);
  _prev_icon->event_handler(&_prev_anchor);
  _prev_icon->parent(this);

  _next_icon = f->create_icon(NAV_NEXT_RGBA, Area(64, 64));
  _next_icon->alpha(100);
  _next_icon->event_handler(&_next_anchor);
  _next_icon->parent(this);
}


void Navbar::next_link(const char *title, Widget *dst)
{
  _next_title = new Block(Block::RIGHT);
  _next_anchor.destination(dst);
  _next_title->append_plaintext(title, &_style);
  append(_next_title);
  append(_next_icon);
}


void Navbar::prev_link(const char *title, Widget *dst)
{
  _prev_title = new Block(Block::LEFT);
  _prev_anchor.destination(dst);
  _prev_title->append_plaintext(title, &_style);
  append(_prev_icon);
  append(_prev_title);
}


void Navbar::draw(Canvas *c, Point const &p)
{
  Rect n = (Rect(_size) + p).offset(ARROW_W, 0, -ARROW_W, 0) & c->clip();

    {
      Mag_gfx::Clip_guard guard(c, n);
      Parent_widget::draw(c, p);
    }

  if (_prev_title)
    _prev_icon->draw(c, _prev_icon->pos() + p);

  if (_next_title)
    _next_icon->draw(c, _next_icon->pos() + p);
}


Widget *
Navbar::find(Point const &p)
{
  Widget *res;

  if (_prev_title && (res = _prev_icon->find(p - _pos)))
    return res;
  if (_next_title && (res = _next_icon->find(p - _pos)))
    return res;

  return Parent_widget::find(p);
}


int Navbar::on_tick()
{
  /* call on_tick function of the fader */
  if (Fader::on_tick() == 0)
    return 0;

  _prev_icon->alpha(_curr_value);
  _next_icon->alpha(_curr_value);
  _style.color = Color(0, 0, 0, _curr_value);

  refresh();
  return 1;
}


Browser *
Navbar::browser() const
{
  for (Widget *e = parent(); e; e = e->parent())
    if (Browser *b = dynamic_cast<Browser*>(e))
      return b;

  return 0;
}

