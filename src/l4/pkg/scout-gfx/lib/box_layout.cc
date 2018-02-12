/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/box_layout>
#include <l4/scout-gfx/layout>

#include "layout_internal.h"
#include <vector>

#include <cstdio>

namespace Scout_gfx {

class Box_layout_p : public Box_layout
{
protected:
  typedef std::vector<Layout_item *> Items;

  Items _items;

protected:
  struct Data
  {
    Rect geometry;

    Area pre_sz;
    Area min_sz;
    Area max_sz;

    Orientations exp;

    int hfw_w;
    int hfw_mh;
    int hfw_h;

    unsigned has_hfw : 1;
    unsigned need_refresh : 1;

    Layout_info::Array info;
  };

  mutable Data _d;

public:
  virtual ~Box_layout_p() {}
  Box_layout_p()
  {
    _d.need_refresh = 1;
    _d.hfw_w = -1;
  }

  Area preferred_size() const;
  Area min_size() const;
  Area max_size() const;

  Orientations expanding() const;
  bool empty() const  { return _items.empty(); }
  bool has_height_for_width() const;
  int height_for_width(int) const;
  int min_height_for_width(int) const;

  void child_invalidate() { invalidate(); }

  void invalidate()
  {
    _d.need_refresh = 1;
    _d.hfw_w = -1;
    Box_layout::invalidate();
  }

  void set_geometry(Rect const &);
  Rect geometry() const { return _d.geometry; }

  void add_item(Scout_gfx::Layout_item *);
  Layout_item *remove_item(int idx);
  Layout_item *item(int idx) const
  { return (idx < (int)_items.size()) ? _items[idx] : 0; }

protected:
  virtual void setup_geometry() const = 0;
  virtual void setup_hfw(int w) const = 0;
  virtual void do_set_geometry(Rect const &r) = 0;

  template<int Hor>
  void _t_setup_geometry() const;

  template<int Hor>
  void _t_setup_hfw(int w) const;
};

class H_box_layout_p : public Box_layout_p
{
public:
  ~H_box_layout_p() {}

protected:
  void setup_geometry() const;
  void setup_hfw(int w) const;
  void do_set_geometry(Rect const &r);

};

class V_box_layout_p : public Box_layout_p
{
public:
  ~V_box_layout_p() {}

protected:
  void setup_geometry() const;
  void setup_hfw(int w) const;
  void do_set_geometry(Rect const &r);

};

Box_layout *
Box_layout::create_horz()
{ return new H_box_layout_p(); }

Box_layout *
Box_layout::create_vert()
{ return new V_box_layout_p(); }


void
Box_layout_p::add_item(Scout_gfx::Layout_item *i)
{
  _items.push_back(i);
  i->set_parent_layout_item(this);
  invalidate();
}

Layout_item *
Box_layout_p::remove_item(int idx)
{
  if (idx >= (int)_items.size())
    return 0;

  Items::iterator x = _items.begin() + idx;
  Layout_item *r = *x;
  _items.erase(x);
  invalidate();
  return r;
}

Area
Box_layout_p::preferred_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.pre_sz;
}

Area
Box_layout_p::min_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.min_sz;
}

Area
Box_layout_p::max_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.max_sz;
}

Orientations
Box_layout_p::expanding() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.exp;
}

bool
Box_layout_p::has_height_for_width() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.has_hfw;
}

int
Box_layout_p::height_for_width(int w) const
{
  if (!has_height_for_width())
    return -1;

  if (w != _d.hfw_w)
    setup_hfw(w);

  return _d.hfw_h + 2 * margin();
}

int
Box_layout_p::min_height_for_width(int w) const
{
  if (!has_height_for_width())
    return -1;

  if (w != _d.hfw_w)
    setup_hfw(w);

  return _d.hfw_mh + 2 * margin();
}



void
Box_layout_p::set_geometry(Rect const &r)
{
  if (_dbg)
    printf("BL[%p] set_geometry(%d,%d - %d,%d)\n", this, r.x1(), r.y1(), r.x2(), r.y2());
  if (r == geometry() && !_d.need_refresh)
    return;

  if (_d.need_refresh)
    setup_geometry();

  if (_dbg)
    printf("BL[%p] set_geometry(ms=%d,%d\n", this, min_size().w(), min_size().h());
  do_set_geometry(r.grow(-margin()));
}

template< int Hor>
void
Box_layout_p::_t_setup_geometry() const
{
  if (!_d.need_refresh)
    return;

  int maxw = 0;
  int maxh = Area::Max_h;
  int minw = 0;
  int minh = 0;
  int hintw = 0;
  int hinth = 0;

  bool horexp = false;
  bool verexp = false;

  _d.has_hfw = false;

  int n = _items.size();
  _d.info.clear();
  Layout_info::Array &a = _d.info;
  a.resize(n);
#if 0
  AreaPolicy::ControlTypes controlTypes1;
  AreaPolicy::ControlTypes controlTypes2;
  int fixedSpacing = q->spacing();
  int previousNonEmptyIndex = -1;

  QStyle *style = 0;
  if (fixedSpacing < 0) {
      if (QWidget *parentWidget = q->parentWidget())
	style = parentWidget->style();
  }
#endif
  for (int i = 0; i < n; i++)
    {
      Layout_item *box = _items[i];
      Area max = box->aligned_max_size();
      Area min = box->min_size();
      Area hint = box->preferred_size();
      Orientations exp = box->expanding();
      bool empty = box->empty();
      int spacing = 0;
#if 0
      if (!empty)
	{
	  if (fixedSpacing >= 0)
	    {
	      spacing = (previousNonEmptyIndex >= 0) ? fixedSpacing : 0;
#ifdef Q_WS_MAC
	      if (!horz(dir) && previousNonEmptyIndex >= 0) {
		  Layout_item *sibling = (dir == QBoxLayout::TopToBottom  ? box : list.at(previousNonEmptyIndex));
		  if (sibling) {
		      QWidget *wid = sibling->item->widget();
		      if (wid)
			spacing = std::max(spacing, sibling->item->geometry().top() - wid->geometry().top());
		  }
	      }
#endif
	    } else {
		controlTypes1 = controlTypes2;
		controlTypes2 = box->item->controlTypes();
		if (previousNonEmptyIndex >= 0) {
		    AreaPolicy::ControlTypes actual1 = controlTypes1;
		    AreaPolicy::ControlTypes actual2 = controlTypes2;
		    if (dir == QBoxLayout::RightToLeft || dir == QBoxLayout::BottomToTop)
		      qSwap(actual1, actual2);

		    if (style) {
			spacing = style->combinedLayoutSpacing(actual1, actual2,
			    horz(dir) ? Qt::Horizontal : Qt::Vertical,
			    0, q->parentWidget());
			if (spacing < 0)
			  spacing = 0;
		    }
		}
	    }

	  if (previousNonEmptyIndex >= 0)
	    a[previousNonEmptyIndex].spacing = spacing;
	  previousNonEmptyIndex = i;
	}
#endif
      bool ignore = empty; // && box->item->widget(); // ignore hidden widgets
      bool dummy = true;
      if (Hor)
	{
	  bool expand = (exp & Horizontal/* || box->stretch > 0*/);
	  horexp = horexp || expand;
	  maxw += spacing + max.w();
	  minw += spacing + min.w();
	  hintw += spacing + hint.w();
	  if (!ignore)
	    max_exp_calc(maxh, verexp, dummy,
		max.h(), exp & Vertical, box->empty());
	  minh = std::max(minh, min.h());
	  hinth = std::max(hinth, hint.h());

	  a[i].pref_sz = hint.w();
	  a[i].max_sz = max.w();
	  a[i].min_sz = min.w();
	  a[i].exp = expand;
	  a[i].stretch = /*box->stretch ? box->stretch : box->hStretch()*/ 0;
	}
      else
	{
	  bool expand = (exp & Vertical/* || box->stretch > 0*/);
	  verexp = verexp || expand;
	  maxh += spacing + max.h();
	  minh += spacing + min.h();
	  hinth += spacing + hint.h();
	  if (!ignore)
	    max_exp_calc(maxw, horexp, dummy,
		max.w(), exp & Horizontal, box->empty());
	  minw = std::max(minw, min.w());
	  hintw = std::max(hintw, hint.w());
	  a[i].pref_sz = hint.h();
	  a[i].max_sz = max.h();
	  a[i].min_sz = min.h();
	  a[i].exp = expand;
	  a[i].stretch = 0; //box->stretch ? box->stretch : box->vStretch();
	}

      a[i].empty = empty;
      a[i].spacing = 0;   // might be be initialized with a non-zero value in a later iteration
      _d.has_hfw = _d.has_hfw || box->has_height_for_width();
    }

  _d.info = a;

  _d.exp = (Orientations)((horexp ? Horizontal : 0)
     | (verexp ? Vertical : 0));

  _d.min_sz = Area(minw, minh);
  _d.max_sz = Area(maxw, maxh).max(_d.min_sz);
  _d.pre_sz = Area(hintw, hinth).max(_d.min_sz).min(_d.max_sz);
#if 0
  q->getContentsMargins(&leftMargin, &topMargin, &rightMargin, &bottomMargin);
  int left, top, right, bottom;
  effectiveMargins(&left, &top, &right, &bottom);
  Area extra(left + right, top + bottom);
#endif
  Area extra(margin()*2, margin()*2);

  _d.min_sz += extra;
  _d.max_sz += extra;
  _d.pre_sz += extra;

  _d.need_refresh = false;
}
namespace {

int __hfw(Layout_item const *i, int w)
{
  if (i->has_height_for_width())
    return i->height_for_width(w);
  else
    return i->preferred_size().h();
}

int __mhfw(Layout_item const *i, int w)
{
  if (i->has_height_for_width())
    return i->min_height_for_width(w);
  else
    return i->min_size().h();
}

}

template< int Hor >
void
Box_layout_p::_t_setup_hfw(int w) const
{
  Layout_info::Array &a = _d.info;

  int n = a.size();
  int h = 0;
  int mh = 0;

  if (Hor)
    {
      Layout_info::calc(a, 0, n, 0, w);
      for (int i = 0; i < n; i++)
	{
	  Layout_item *box = _items[i];
	  h = std::max(h, __hfw(box, a[i].size));
	  mh = std::max(mh, __mhfw(box, a[i].size));
	}
    }
  else
    {
      int xw = w - 2 * margin();
      for (int i = 0; i < n; ++i)
	{
	  Layout_item *box = _items[i];
	  int spacing = a[i].spacing;
	  h += __hfw(box, xw);
	  mh += __mhfw(box, xw);
	  h += spacing;
	  mh += spacing;
	}
    }
  _d.hfw_w = w;
  _d.hfw_h = h;
  _d.hfw_mh = mh;
}


void
H_box_layout_p::setup_hfw(int w) const
{ _t_setup_hfw<true>(w); }

void
H_box_layout_p::setup_geometry() const
{ _t_setup_geometry<true>(); }

void
H_box_layout_p::do_set_geometry(Rect const &r)
{
  Layout_info::Array &a = _d.info;
  int pos = r.x1();
  int space = r.w();
  int n = a.size();

  Layout_info::calc(a, 0, n, pos, space);

  for (int i = 0; i < n; i++)
    {
      Layout_item *box = _items[i];

      box->aligned_set_geometry(Rect(Point(a[i].pos, r.y1()), Area(a[i].size, r.h())));
    }
}


void
V_box_layout_p::setup_hfw(int w) const
{ _t_setup_hfw<false>(w); }

void
V_box_layout_p::setup_geometry() const
{ _t_setup_geometry<false>(); }

void
V_box_layout_p::do_set_geometry(Rect const &r)
{
  Layout_info::Array &a = _d.info;
  int pos = r.y1();
  int space = r.h();
  int n = a.size();

  if (_d.has_hfw)
    {
      for (int i = 0; i < n; i++)
	{
	  Layout_item *box = _items[i];
	  if (box->has_height_for_width())
	    a[i].pref_sz = a[i].min_sz =
	      box->height_for_width(r.w());
	}
    }

  Layout_info::calc(a, 0, n, pos, space);

  for (int i = 0; i < n; i++)
    {
      Layout_item *box = _items[i];
      box->aligned_set_geometry(Rect(Point(r.x1(), a[i].pos), Area(r.w(), a[i].size)));
    }
}

}
