/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/mag-gfx/clip_guard>
#include <l4/scout-gfx/scroll_pane>
#include <l4/scout-gfx/scrollbar>
#include <l4/scout-gfx/layout>

namespace Scout_gfx {

Scroll_pane::Scroll_pane(Factory *f)
: _hsb(new Scrollbar(f, Horizontal)), _vsb(new Scrollbar(f)), _texture(0),
  _exp(Vertical), _sbvis(0),
  _sbl(this), _vp_layout(this),
  _decouple_view_pane(false)
{
  if (_vsb)
    {
      _vsb->parent(this);
      _vsb->listener(&_sbl);
      _vsb->visibility(true);
    }

  if (_hsb)
    {
      _hsb->parent(this);
      _hsb->listener(&_sbl);
      _hsb->visibility(true);
    }
}

Scroll_pane::Vp_l::Vp_l(Scroll_pane *p)
: _p(p), _size(0,0), _pos(0,0), 
  _min_csize(32, 32),
  _max_csize(Area::Max_w, Area::Max_h),
  _pref_csize(Area::Max_w, Area::Max_h),
  _sbres(0,0)
{}

Area
Scroll_pane::Vp_l::min_size() const
{
  Area s = _min_csize;
  Area vs = _p->have_vsb() ? _p->_vsb->min_size() : Area(0,0);
  Area hs = _p->have_hsb() ? _p->_hsb->min_size() : Area(0,0);
  s += Area(vs.w(), hs.h());
  s = s.max(Area(hs.w(), vs.h()));
  return s;
}

Area
Scroll_pane::Vp_l::preferred_size() const
{
  Area s;
  Layout *l = _p->child_layout();
  if (l)
    s = _pref_csize.min(l->preferred_size());
  else
    s = _min_csize;

  Area vs = _p->have_vsb() ? _p->_vsb->min_size() : Area(0,0);
  Area hs = _p->have_hsb() ? _p->_hsb->min_size() : Area(0,0);
  s += Area(vs.w(), hs.h());
  s = s.max(Area(hs.w(), vs.h()));
  return s;
}

Area
Scroll_pane::Vp_l::max_size() const
{
  Area s = _max_csize;
  Area vs = _p->have_vsb() ? _p->_vsb->min_size() : Area(0,0);
  Area hs = _p->have_hsb() ? _p->_hsb->min_size() : Area(0,0);
  s += Area(vs.w(), hs.h());
  s = s.max(Area(hs.w(), vs.h()));
  return s;
}

void
Scroll_pane::Vp_l::set_geometry(Rect const &r)
{
  Area s = r.area().max(min_size());

  bool vsb = _p->have_vsb();
  bool hsb = _p->have_hsb();

  Area vsbs = vsb ? _p->_vsb->min_size() : Area(0,0);
  Area hsbs = hsb ? _p->_hsb->min_size() : Area(0,0);

  Layout *l = _p->child_layout();
  if (!l)
    {
      if (vsb)
	_p->_vsb->set_geometry(Rect(Point(s.w() - vsbs.w(), 0) + r.p1(), Area(vsbs.w(), s.h())));
      if (hsb)
	_p->_hsb->set_geometry(Rect(Point(0, s.h() - hsbs.h()) + r.p1(), Area(s.w(), hsbs.h())));
    }

  _pos = r.p1();
  _size = s;

  if (!l)
    return;

  Area vsb_ms = _p->_vsb ? _p->_vsb->min_size() : Area(0,0);
  Area hsb_ms = _p->_hsb ? _p->_hsb->min_size() : Area(0,0);
  Area min_pane = l->min_size();

  int pref_w = std::max(min_pane.w(), s.w());
  int ch = l->height_for_width(pref_w);
  Area useful_pane = s;
  if (pref_w > s.w() && _p->_hsb)
    {
      useful_pane -= Area(0, hsb_ms.h());
      hsb = true;
    }
  else
    hsb = false;

  if (ch >= 0)
    {
      if (ch > useful_pane.h() && _p->_vsb)
	{
	  useful_pane -= Area(vsb_ms.w(), 0);
	  pref_w = std::max(min_pane.w(), useful_pane.w());
	  ch = l->height_for_width(pref_w);
	  vsb = true;
	  if (pref_w > useful_pane.w() && _p->_hsb)
	    {
	      useful_pane -= Area(0, hsb_ms.h());
	      hsb = true;
	    }
	}
      else
	vsb = false;
    }
  else
    {
      ch = l->preferred_size().h();
      if (ch > useful_pane.h() && _p->_vsb)
	{
	  useful_pane -= Area(_p->_vsb->min_size().w(), 0);
	  vsb = true;
	}
      else
	vsb = false;
    }

  _p->_sbvis = 0;
  if (hsb)
    {
      _p->_sbvis |= Horizontal;
      hsbs = hsb_ms;
    }
  if (vsb)
    {
      _p->_sbvis |= Vertical;
      vsbs = vsb_ms;
    }

  bool inval = false;
  if (_p->_hsb)
    {
      inval |= (_p->_hsb->visible() != hsb);
      _p->_hsb->visibility(hsb);
    }
  if (_p->_vsb)
    {
      inval |= (_p->_vsb->visible() != vsb);
      _p->_vsb->visibility(vsb);
    }


  int vo = vsb ? _p->_vsb->view_pos() : 0;
  int ho = hsb ? _p->_hsb->view_pos() : 0;

  if (inval)
    invalidate();

  l->set_geometry(Rect(Point(-ho, -vo) + r.p1(), Area(pref_w, ch)));
  if (vsb)
    {
      _p->_vsb->view(ch, useful_pane.h(), vo);
      _p->_vsb->set_geometry(Rect(Point(s.w() - vsbs.w(), 0) + r.p1(), Area(vsbs.w(), s.h() - std::max(hsbs.h(), _sbres.h()))));
    }

  if (hsb)
    {
      _p->_hsb->view(pref_w, useful_pane.w(), ho);
      _p->_hsb->set_geometry(Rect(Point(0, s.h() - hsbs.h()) + r.p1(), Area(s.w() - std::max(vsbs.w(), _sbres.w()), hsbs.h())));
    }
}

void
Scroll_pane::set_texture_geometry(Rect const &r)
{
  if (!_texture)
    return;

  Point offs(_hsb ? _hsb->view_pos() : 0, _vsb ? _vsb->view_pos() : 0);

  _texture->set_geometry(Rect(r.p1() - offs, r.area() + Area(+offs.x(), +offs.y())));

}

void
Scroll_pane::refresh_geometry()
{
  set_texture_geometry(Rect(_size));
  _vp_layout.set_geometry(_vp_layout.geometry());
  refresh();
}


void
Scroll_pane::set_geometry(Rect const &r)
{
  Area s = r.area().max(min_size());
  _size = s;
  _pos = r.p1();
  set_texture_geometry(Rect(_size));
  if (!_decouple_view_pane)
    _vp_layout.set_geometry(Rect(_pos, _size));
}

Area 
Scroll_pane::preferred_size() const
{ return _vp_layout.preferred_size(); }

Area
Scroll_pane::min_size() const
{ return _vp_layout.min_size(); }

Area 
Scroll_pane::max_size() const
{ return _vp_layout.max_size(); }

Widget *
Scroll_pane::find(Point const &p)
{
  Rect g = _vp_layout.geometry();
  if (!g.contains(p))
    return 0;

  Point np = p - geometry().p1();

  Widget *e;
  if (have_vsb() && (e = _vsb->find(np)))
    return e;

  if (have_hsb() && (e = _hsb->find(np)))
    return e;

  if (child_layout())
    return Parent_widget::find(p + child_layout()->geometry().p1());

  return 0;
}

void
Scroll_pane::draw(Mag_gfx::Canvas *c, Point const &p)
{
  Mag_gfx::Clip_guard g(c, Rect(p, _size));

  if (_texture)
    _texture->draw(c, p + static_cast<Layout_item*>(_texture)->geometry().p1());

  Parent_widget::draw(c, p);
   if (_vsb)
     _vsb->draw(c, p + _vsb->geometry().p1());
   if (_hsb)
     _hsb->draw(c, p + _hsb->geometry().p1());
}

void
Scroll_pane::child_invalidate()
{
  refresh_geometry();
}

}

