/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/stack_layout>
#include "layout_internal.h"

namespace Scout_gfx {

class Stack_layout_p : public Stack_layout
{
public:
  typedef std::vector<Layout_item *> Items;

  Items _items;
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
    unsigned need_distribute : 1;

    Data()
    : hfw_w(-1), need_refresh(true), need_distribute(true)
    {}
  };

  mutable Data _d;

public:
  virtual ~Stack_layout_p() {}

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
    _d.need_distribute = 1;
    Stack_layout::invalidate();
  }

  void set_geometry(Rect const &);
  Rect geometry() const { return _d.geometry; }

  void add_item(Scout_gfx::Layout_item *);
  Layout_item *remove_item(int idx);
  Layout_item *item(int idx) const
  { return (idx < (int)_items.size()) ? _items[idx] : 0; }

protected:
  void setup_geometry() const;
  void setup_hfw(int w) const;
};


void
Stack_layout_p::setup_geometry() const
{
  if (!_d.need_refresh)
    return;

  _d.min_sz = Area(0,0);
  _d.max_sz = Area(Area::Max_w, Area::Max_h);
  _d.pre_sz = Area(0,0);

  for (Items::const_iterator i = _items.begin(); i != _items.end(); ++i)
    {
      Layout_item *b = *i;
      _d.min_sz = _d.min_sz.max(b->min_size());
      _d.max_sz = _d.max_sz.min(b->max_size());
      _d.pre_sz = _d.pre_sz.max(b->preferred_size());
      _d.exp |= b->expanding();
      _d.has_hfw |= b->has_height_for_width();
    }

  _d.min_sz += Area(margin(), margin());
  _d.max_sz += Area(margin(), margin());
  _d.pre_sz += Area(margin(), margin());

  _d.need_refresh = 0;
}

void
Stack_layout_p::setup_hfw(int w) const
{
  _d.hfw_h = 0;
  _d.hfw_mh = 0;
  _d.hfw_w = w;

  for (Items::const_iterator i = _items.begin(); i != _items.end(); ++i)
    {
      Layout_item *b = *i;
      if (b->has_height_for_width())
	{
	  _d.hfw_h = std::max(_d.hfw_h, b->height_for_width(w));
	  _d.hfw_mh = std::max(_d.hfw_mh, b->min_height_for_width(w));
	}
      else
	{
	  _d.hfw_h = std::max(_d.hfw_h, b->preferred_size().h());
	  _d.hfw_mh = std::max(_d.hfw_mh, b->min_size().h());
	}
    }
}


Stack_layout *
Stack_layout::create()
{ return new Stack_layout_p(); }
void
Stack_layout_p::add_item(Scout_gfx::Layout_item *i)
{
  _items.push_back(i);
  i->set_parent_layout_item(this);
  invalidate();
}

Layout_item *
Stack_layout_p::remove_item(int idx)
{
  if (idx >= (int)_items.size())
    return 0;

  Items::iterator x = _items.begin() + idx;
  Layout_item *r = *x;
  _items.erase(x);
  r->set_parent_layout_item(0);
  invalidate();
  return r;
}

Area
Stack_layout_p::preferred_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.pre_sz;
}

Area
Stack_layout_p::min_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.min_sz;
}

Area
Stack_layout_p::max_size() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.max_sz;
}

Orientations
Stack_layout_p::expanding() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.exp;
}

bool
Stack_layout_p::has_height_for_width() const
{
  if (_d.need_refresh)
    setup_geometry();

  return _d.has_hfw;
}

int
Stack_layout_p::height_for_width(int w) const
{
  if (!has_height_for_width())
    return -1;

  if (w != _d.hfw_w)
    setup_hfw(w);

  return _d.hfw_h + 2 * margin();
}

int
Stack_layout_p::min_height_for_width(int w) const
{
  if (!has_height_for_width())
    return -1;

  if (w != _d.hfw_w)
    setup_hfw(w);

  return _d.hfw_mh + 2 * margin();
}



void
Stack_layout_p::set_geometry(Rect const &r)
{
  if (r == geometry() && !_d.need_refresh && !_d.need_distribute)
    return;

  if (_d.need_refresh)
    setup_geometry();

  Rect s(r.p1(), r.area().max(min_size()).min(max_size()));
  _d.geometry = s;

  s = s.grow(-margin());
  for (Items::iterator i = _items.begin(); i != _items.end(); ++i)
    {
      Layout_item *b = *i;
      b->aligned_set_geometry(s);
    }
  _d.need_distribute = false;
}

}

