/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/grid_layout>
#include <l4/scout-gfx/layout>

#include "layout_internal.h"
#include <vector>

namespace Scout_gfx {

class G_box
{
public:
  explicit G_box(Layout_item *i) : _i(i) {}

  Area preferred_size() const { return _i->preferred_size(); }
  Area max_size() const { return _i->aligned_max_size(); }
  Area min_size() const { return _i->min_size(); }
  Orientations expanding() const { return _i->expanding(); }
  bool empty() const { return _i->empty(); }
  bool has_height_for_width() const { return _i->has_height_for_width(); }
  int height_for_width(int w) const { return _i->height_for_width(w); }
  int min_height_for_width(int w) const { return _i->min_height_for_width(w); }
  Mag_gfx::Alignment alignment() const  { return _i->alignment(); }
  void set_alignment(Mag_gfx::Alignment a) { _i->set_alignment(a); }
  void set_geometry(Rect const &g) { _i->aligned_set_geometry(g); }
  Rect geometry() const { return _i->geometry(); }

  Layout_item *item() const { return _i; }


  int h_stretch() const { return 0; }
  int v_stretch() const { return 0; }

private:
  friend class G_layout_d;

  Layout_item *_i;
  int r;
  int c;
  int tr;
  int tc;
};

class G_layout_d
{
public:
  int i_spacing;
  int o_border;
  unsigned top_level : 1;
  unsigned enabled : 1;
  unsigned activated : 1;
  //uint autoNewChild : 1;
  //QLayout::SizeConstraint constraint;
  Rect rect;


  G_layout_d();

  void add(G_box *, int r, int c);
  void add(G_box *, int r1, int r2, int c1, int c2);
  Area preferred_size(int sp) const;
  Area min_size(int sp) const;
  Area max_size(int sp) const;

  Orientations expanding(int spacing) const;
  void distribute(Rect, int);
  int n_rows() const { return _rc; }
  int n_cols() const { return _cc; }

  void expand(int r, int c)
  { set_size(std::max(r, _rc), std::max(c, _cc)); }

  void set_row_stretch(int r, int s)
  {
    expand(r + 1, 0);
    _r_str[r] = s;
    invalidate();
  }

  void set_col_stretch(int c, int s)
  {
    expand(0, c + 1);
    _c_str[c] = s;
    invalidate();
  }

  int row_stretch(int r) const { return _r_str[r]; }
  int col_stretch(int c) const { return _c_str[c]; }

  void set_row_spacing(int r, int s)
  {
    expand(r + 1, 0);
    _r_sp[r] = s;
    invalidate();
  }

  void set_col_spacing(int c, int s)
  {
    expand(0, c + 1);
    _c_sp[c] = s;
    invalidate();
  }

  int row_spacing(int r) const { return _r_sp[r]; }
  int col_spacing(int c) const { return _c_sp[c]; }

  void set_rev(bool r, bool c)
  {
    _h_rev = c;
    _v_rev = r;
  }

  bool horz_reversed() const { return _h_rev; }
  bool vert_reversed() const { return _v_rev; }

  void invalidate()
  {
    _need_refresh = true;
    _need_distribute = true;
    _hfw_w = -1;
  }

  bool need_refresh() const { return _need_refresh || _need_distribute; }

  bool has_height_for_width(int space);
  int height_for_width(int, int, int);
  int min_height_for_width(int, int, int);

  void next_pos(int *r, int *c)
  {
    *r = _n_r;
    *c = _n_c;
  }

  int size() const { return _t.size(); }
  Rect cell_rect(int r, int c) const;

  Layout_item *item(int idx) const
  {
    if (idx < (int)_t.size())
      return _t[idx]->item();
    else
      return 0;
  }

  Layout_item *remove_item(int idx)
  {
    if (idx >= (int)_t.size())
      return 0;

    Items::iterator x = _t.begin() + idx;
    G_box *b = *x;
    _t.erase(x);

    if (!b)
      return 0;

    Layout_item *i = b->item();
    delete b;

    i->set_parent_layout_item(0);
    invalidate();
    return i;
  }

  void get_pos(int idx, int *r, int *c, int *rspawn, int *cspawn)
  {
    if (idx >= (int)_t.size())
      return;

    G_box *b = _t[idx];

    int _tor = b->tr < 0 ? _rc - 1 : b->tr;
    int _toc = b->tc < 0 ? _cc - 1 : b->tc;

    *r = b->r;
    *c = b->c;
    *rspawn = _tor - *r + 1;
    *cspawn = _toc - *c + 1;
  }

  void clear();

private:
  void set_next_pos_after(int r, int c);
  void recalc_hfw(int w, int s);
  void add_hfw_data(G_box *b, int w);
  void init();
  Area find_size(int Layout_info::*, int) const;
  void add_data(G_box *, bool r = true, bool c = true);
  void set_size(int r, int c);
  void setup_layout(int space);
  void setup_hfw(int space);


  int _rc;
  int _cc;

  Layout_info::Array _rd;
  Layout_info::Array _cd;
  Layout_info::Array *_hfwd;
  std::vector<int> _r_str;
  std::vector<int> _c_str;
  std::vector<int> _r_sp;
  std::vector<int> _c_sp;

  typedef std::vector<G_box*> Items;
  Items _t;

  int _hfw_w;
  int _hfw_h;
  int _hfw_min_h;
  int _n_r;
  int _n_c;

  friend class Grid_layout;

  unsigned _h_rev : 1;
  unsigned _v_rev : 1;
  unsigned _need_refresh : 1;
  unsigned _need_distribute : 1;
  unsigned _has_hfw : 1;
  unsigned _add_vert : 1;
};

G_layout_d::G_layout_d()
: i_spacing(-1), o_border(-1), top_level(false),
  enabled(true), activated(true),
  _rc(0), _cc(0), _hfwd(0), _hfw_w(-1), _n_r(0), _n_c(0),
  _h_rev(false), _v_rev(false),
  _need_refresh(true), _need_distribute(true)
{}

void
G_layout_d::clear()
{
  for (Items::iterator i = _t.begin(); i != _t.end(); ++i)
    if (*i)
      {
	(*i)->item()->set_parent_layout_item(0);
	delete *i;
      }

  _t.clear();
  if (_hfwd)
    delete _hfwd;
}

bool
G_layout_d::has_height_for_width(int space)
{
  const_cast<G_layout_d*>(this)->setup_layout(space);
  return _has_hfw;
}

void
G_layout_d::recalc_hfw(int w, int spacing)
{
  if (!_hfwd)
    _hfwd = new Layout_info::Array(_rc); 

  setup_hfw(spacing);
  Layout_info::Array &rd = *_hfwd;

  int h = 0, mh = 0, n = 0;

  for (int r = 0; r < _rc; ++r)
    {
      h += rd[r].pref_sz;
      mh += rd[r].min_sz;
      if (!rd[r].empty)
	++n;
    }

  if (n)
    {
      h += (n - 1) * spacing;
      mh += (n - 1) * spacing;
    }

  _hfw_w = w;
  _hfw_h = std::min<int>(Area::Max_h, h);
  _hfw_min_h = std::min<int>(Area::Max_h, mh);
}

int
G_layout_d::height_for_width(int w, int margin, int spacing)
{
  setup_layout(spacing);
  if (!_has_hfw)
    return -1;

  if (w + 2 * margin != _hfw_w)
    {
      Layout_info::calc(_cd, 0, _cc, 0, w - 2 * margin, spacing);
      recalc_hfw(w - 2* margin, spacing);
    }
  return _hfw_h + 2 * margin;
}


int
G_layout_d::min_height_for_width(int w, int margin, int spacing)
{
  height_for_width(w, margin, spacing);
  return _has_hfw ? (_hfw_min_h + 2 * margin) : -1;
}

Area
G_layout_d::find_size(int Layout_info::*size, int spacer) const
{
  G_layout_d *self = const_cast<G_layout_d*>(this);
  self->setup_layout(spacer);

  int w = 0, h = 0, n = 0;

  for (int r = 0; r < _rc; ++r)
    {
      h += _rd[r].*size;
      if (!_rd[r].empty)
	++n;
    }

  if (n)
    h += (n - 1) * spacer;

  n = 0;
  for (int c = 0; c < _cc; ++c)
    {
      w += _cd[c].*size;
      if (!_cd[c].empty)
	++n;
    }

  if (n)
    w += (n - 1) * spacer;

  w = std::min<int>(Area::Max_w, w);
  h = std::min<int>(Area::Max_h, h);
   
  return Area(w, h);
}

Orientations
G_layout_d::expanding(int spacing) const
{
  G_layout_d *self = const_cast<G_layout_d*>(this);
  self->setup_layout(spacing);

  Orientations res;

  for (int r = 0; r < _rc; ++r)
    if (_rd[r].exp)
      {
	res |= Vert;
	break;
      }

  for (int c = 0; c < _cc; ++c)
    if (_cd[c].exp)
      {
	res |= Horz;
	break;
      }
    
  return res;
}

Area
G_layout_d::preferred_size(int sp) const
{ return find_size(&Layout_info::pref_sz, sp); }

Area
G_layout_d::min_size(int sp) const
{ return find_size(&Layout_info::min_sz, sp); }

Area
G_layout_d::max_size(int sp) const
{ return find_size(&Layout_info::max_sz, sp); }

void
G_layout_d::set_size(int r, int c)
{
  if ((int)_rd.size() < r)
    {
      int new_r = std::max(r, _rc * 2);
      _rd.resize(new_r);
      _r_str.resize(new_r);
      _r_sp.resize(new_r);
      for (int i = _rc; i < new_r; ++i)
	{
	  Layout_info &d = _rd[i];
	  d.init();
	  d.max_sz = 0;
	  d.pos = 0;
	  d.size = 0;
	  _r_str[i] = 0;
	  _r_sp[i] = 0;
	}
    }

  if ((int)_cd.size() < c)
    {
      int new_c = std::max(c, _cc * 2);
      _cd.resize(new_c);
      _c_str.resize(new_c);
      _c_sp.resize(new_c);
      for (int i = _cc; i < new_c; ++i)
	{
	  Layout_info &d = _cd[i];
	  d.init();
	  d.max_sz = 0;
	  d.pos = 0;
	  d.size = 0;
	  _c_str[i] = 0;
	  _c_sp[i] = 0;
	}
    }

  _rc = r;
  _cc = c;
}

void
G_layout_d::set_next_pos_after(int r, int c)
{
  if (_add_vert)
    {
      if (c > _n_c || (c == _n_c && r >= _n_r))
	{
	  _n_r = r + 1;
	  _n_c = c;
	  if (_n_r >= _rc)
	    {
	      _n_r = 0;
	      ++_n_c;
	    }
	}
    }
  else
    {
      if (r > _n_r || (r == _n_r && c >= _n_c))
	{
	  _n_r = r;
	  _n_c = c + 1;
	  if (_n_c >= _cc)
	    {
	      _n_c = 0;
	      ++_n_r;
	    }
	}
    }
}

void
G_layout_d::add(G_box *b, int r, int c)
{
  expand(r + 1, c + 1);
  b->r = b->tr = r;
  b->c = b->tc = c;
  _t.push_back(b);
  invalidate();
  set_next_pos_after(r, c);
}

void
G_layout_d::add(G_box *b, int r1, int r2, int c1, int c2)
{
  expand(r2 + 1, c2 + 1);
  b->r = r1;
  b->tr = r2;
  b->c = c1;
  b->tc = c2;
  _t.push_back(b);
  invalidate();

  if (c2 < 0)
    c2 = _cc - 1;

  set_next_pos_after(r2, c2);
}


void
G_layout_d::add_data(G_box *b, bool r, bool c)
{
  Area prs = b->preferred_size();
  Area mis = b->min_size();
  Area mas = b->max_size();

  if (b->empty())
    return;

  if (c)
    {
      Layout_info &cd = _cd[b->c];
      if (!_c_str[b->c])
	cd.stretch = std::max(cd.stretch, b->h_stretch());

      cd.pref_sz = std::max(prs.w(), cd.pref_sz);
      cd.min_sz = std::max(mis.w(), cd.min_sz);

      bool ex = cd.exp;
      bool em = cd.empty;
      max_exp_calc(cd.max_sz, ex, em, mas.w(), b->expanding() & Horz, b->empty());
      cd.exp = ex;
      cd.empty = em;
    }

  if (r)
    {
      Layout_info &rd = _rd[b->r];
      if (_r_str[b->r])
	rd.stretch = std::max(rd.stretch, b->v_stretch());

      rd.pref_sz = std::max(prs.h(), rd.pref_sz);
      rd.min_sz = std::max(mis.h(), rd.min_sz);
      bool ex = rd.exp;
      bool em = rd.empty;
      max_exp_calc(rd.max_sz, ex, em, mas.h(), b->expanding() & Vert, b->empty());
      rd.exp = ex;
      rd.empty = em;
    }

}

namespace {
static void
dmb(Layout_info::Array &chain, int spacing, int start, int end,
    int min_sz, int pref_sz, std::vector<int> &stretcha, int stretch)
{
  int i;
  int w = 0;
  int wh = 0;
  int max = 0;
  for (i = start; i <= end; ++i)
    {
      w += chain[i].min_sz;
      wh += chain[i].pref_sz;
      if (chain[i].empty)
	chain[i].max_sz = Area::Max_w;
      max += chain[i].max_sz;
      chain[i].empty = false;
      if (stretcha[i] == 0)
	chain[i].stretch = std::max(chain[i].stretch, stretch);
    }
  w += spacing * (end - start);
  wh += spacing * (end - start);
  max += spacing * (end - start);

  if (max < min_sz)
    { // implies w < minSize
      /*
	 We must increase the maximum size of at least one of the
	 items. qGeomCalc() will put the extra space in between the
	 items. We must recover that extra space and put it
	 somewhere. It does not really matter where, since the user
	 can always specify stretch factors and avoid this code.
       */
      Layout_info::calc(chain, start, end - start + 1, 0, min_sz, spacing);
      int pos = 0;
      for (i = start; i <= end; ++i)
	{
	  int next_p = (i == end) ? min_sz - 1 : chain[i + 1].pos;
	  int real_sz = next_p - pos;
	  if (i != end)
	    real_sz -= spacing;
	  if (chain[i].min_sz < real_sz)
	    chain[i].min_sz = real_sz;
	  if (chain[i].max_sz < chain[i].min_sz)
	    chain[i].max_sz = chain[i].min_sz;
	  pos = next_p;
	}
    }
  else if (w < min_sz)
    {
      Layout_info::calc(chain, start, end - start + 1, 0, min_sz, spacing);
      for (i = start; i <= end; ++i)
	if (chain[i].min_sz < chain[i].size)
	  chain[i].min_sz = chain[i].size;
    }

  if (wh < pref_sz)
    {
      Layout_info::calc(chain, start, end - start + 1, 0, pref_sz, spacing);
      for (i = start; i <= end; ++i)
	if (chain[i].pref_sz < chain[i].size)
	  chain[i].pref_sz = chain[i].size;
    }
}

static void
setup_data(Layout_info::Array &d, int cnt, std::vector<int> const &str,
           std::vector<int> const &sp)
{
  for (int i = 0; i < cnt; ++i)
    {
      Layout_info &_d = d[i];
      int _st = str[i];
      int _sp = sp[i];
      _d.init(_st, _sp);
      _d.max_sz = _st ? (int)Area::Max_h : _sp;
    }
}


static void
set_expansion(Layout_info::Array &d, int cnt)
{
  for (int i = 0; i < cnt; ++i)
    d[i].exp = d[i].exp || d[i].stretch > 0;
}

}

void
G_layout_d::setup_layout(int spacing)
{
  if (!_need_refresh)
    return;

  _has_hfw = false;

  setup_data(_rd, _rc, _r_str, _r_sp);
  setup_data(_cd, _cc, _c_str, _c_sp);

  for (int pass = 0; pass < 2; ++pass)
    {
      for (unsigned i = 0; i < _t.size(); ++i)
	{
	  G_box *box = _t[i];
	  int r1 = box->r;
	  int c1 = box->c;
	  int r2 = box->tr;
	  int c2 = box->tc;
	  if (r2 < 0)
	    r2 = _rc - 1;
	  if (c2 < 0)
	    c2 = _cc - 1;

	  Area pref = box->preferred_size();
	  Area min = box->min_size();
	  if (box->has_height_for_width())
	    _has_hfw = true;

	  if (r1 == r2)
	    {
	      if (pass == 0)
		add_data(box, true, false);
	    }
	  else
	    {
	      if (pass == 1)
		dmb(_rd, spacing, r1, r2, min.h(), pref.h(),
		    _r_str, box->v_stretch());
	    }
	  if (c1 == c2)
	    {
	      if (pass == 0)
		add_data(box, false, true);
	    }
	  else
	    {
	      if (pass == 1)
		dmb(_cd, spacing, c1, c2, min.w(), pref.w(),
		    _c_str, box->h_stretch());
	    }
	}
    }

  set_expansion(_rd, _rc);
  set_expansion(_cd, _cc);

  _need_refresh = false;
}

void
G_layout_d::add_hfw_data(G_box *box, int w)
{
  Layout_info::Array &rd = *_hfwd;
  Layout_info &d = rd[box->r];
  if (box->has_height_for_width())
    {
      int pref = box->height_for_width(w);
      d.pref_sz = std::max(pref, d.pref_sz);
      d.min_sz = std::max(pref, d.min_sz);
    }
  else
    {
      Area pref = box->preferred_size();
      Area mis = box->min_size();
      d.pref_sz = std::max(pref.h(), d.pref_sz);
      d.min_sz = std::max(mis.h(), d.min_sz);
    }
}


void
G_layout_d::setup_hfw(int spacing)
{
  Layout_info::Array &rd = *_hfwd;
  for (int i = 0; i < _rc; ++i)
    {
      Layout_info &d = rd[i];
      d = _rd[i];
      d.min_sz = d.pref_sz = _r_sp[i];
    }

  for (int pass = 0; pass < 2; ++pass)
    {
      for (unsigned i = 0; i < _t.size(); ++i)
	{
	  G_box *box = _t[i];
	  int r1 = box->r;
	  int c1 = box->c;
	  int r2 = box->tr;
	  int c2 = box->tc;
	  if (r2 < 0)
	    r2 = _rc-1;
	  if (c2 < 0)
	    c2 = _cc-1;

	  int w = _cd[c2].pos + _cd[c2].size - _cd[c1].pos;

	  if (r1 == r2)
	    {
	      if (pass == 0)
		add_hfw_data(box, w);
	    }
	  else
	    {
	      if (pass == 1)
		{
		  Area pref = box->preferred_size();
		  Area min = box->min_size();
		  if (box->has_height_for_width())
		    {
		      int hfwh = box->height_for_width(w);
		      if (hfwh > pref.h())
			pref.h(hfwh);
		      if (hfwh > min.h())
			min.h(hfwh);
		    }
		  dmb(rd, spacing, r1, r2, min.h(), pref.h(),
		      _r_str, box->v_stretch());
		}
	    }
	}
    }

  set_expansion(rd, _rc);
}


void
G_layout_d::distribute(Rect r, int spacing)
{
  bool visual_h_rev = _h_rev;
#if 0
  Layout_item *parent = parent_layout_item();
  if (parent && parent->is_right_to_left())
    visual_h_rev = !visual_h_rev;
#endif
  setup_layout(spacing);

  Layout_info::calc(_cd, 0, _cc, r.x1(), r.w(), spacing);
  Layout_info::Array *rdp;
  if (_has_hfw)
    {
      recalc_hfw(r.w(), spacing);
      Layout_info::calc(*_hfwd, 0, _rc, r.y1(), r.h(), spacing);
      rdp = _hfwd;
    }
  else
    {
      Layout_info::calc(_rd, 0, _rc, r.y1(), r.h(), spacing);
      rdp = &_rd;
    }

  Layout_info::Array &rd = *rdp;

  bool reverse = ((r.y2() > rect.y2()) || (r.y2() == rect.y2()
	&& ((r.x2() > rect.x2()) != visual_h_rev)));
  int n = _t.size();
  for (int i = 0; i < n; ++i)
    {
      G_box *box = _t[reverse ? n-i-1 : i];

      int r2 = box->tr;
      int c2 = box->tc;
      if (r2 < 0)
	r2 = _rc-1;
      if (c2 < 0)
	c2 = _cc-1;

      int x = _cd[box->c].pos;
      int y = rd[box->r].pos;
      int x2p = _cd[c2].pos + _cd[c2].size; // x2+1
      int y2p = rd[r2].pos + rd[r2].size;    // y2+1
      int w = x2p - x;
      int h = y2p - y;

      if (visual_h_rev)
	x = r.x1() + r.x2() - x - w + 1;
      if (_v_rev)
	y = r.y1() + r.y2() - y - h + 1;

      box->set_geometry(Rect(Point(x, y), Area(w, h)));
    }
  _need_distribute = false;
}

Rect
G_layout_d::cell_rect(int row, int col) const
{
  if (row < 0 || row >= _rc || col < 0 || col >= _cc)
    return Rect();

  Layout_info::Array const *rdp;
  if (_has_hfw && _hfwd)
    rdp = _hfwd;
  else
    rdp = &_rd;

  return Rect(Point(_cd[col].pos, (*rdp)[row].pos),
              Area(_cd[col].size, (*rdp)[row].size));
}

class G_layout_p : public Layout, private G_layout_d
{
public:
  G_layout_p() { expand(1,1); }
  ~G_layout_p();
};

Grid_layout::Grid_layout()
: _d(new G_layout_d())
{
  _d->expand(1,1);
}

Grid_layout::~Grid_layout()
{
  _d->clear();
  delete _d;
}

void
Grid_layout::set_default_positioning(int n, Orientation orient)
{
  if (orient == Horizontal)
    {
      _d->expand(1, n);
      _d->_add_vert = false;
    }
  else
    {
      _d->expand(n, 1);
      _d->_add_vert = true;
    }
}

int
Grid_layout::row_count() const { return _d->n_rows(); }

int
Grid_layout::col_count() const { return _d->n_cols(); }

Area
Grid_layout::preferred_size() const
{
  int m = margin();
  return _d->preferred_size(spacing()) + Area(2 * m, 2 * m);
}

Area
Grid_layout::min_size() const
{
  int m = margin();
  return _d->min_size(spacing()) + Area(2 * m, 2 * m);
}

Area
Grid_layout::max_size() const
{
  int m = margin();
  Area s = _d->max_size(spacing()) + Area(2 * m, 2 * m);
  s = s.min(Area(Area::Max_w, Area::Max_h));
  return s;
}

bool
Grid_layout::has_height_for_width() const
{ return _d->has_height_for_width(spacing()); }

int
Grid_layout::height_for_width(int w) const
{ return _d->height_for_width(w, margin(), spacing()); }

int
Grid_layout::min_height_for_width(int w) const
{ return _d->min_height_for_width(w, margin(), spacing()); }

int
Grid_layout::count() const
{ return _d->size(); }

Layout_item *
Grid_layout::item(int idx) const
{ return _d->item(idx); }

Layout_item *
Grid_layout::remove_item(int idx)
{ return _d->remove_item(idx); }


void
Grid_layout::get_item_pos(int idx, int *row, int *column,
                          int *row_span, int *column_span)
{ _d->get_pos(idx, row, column, row_span, column_span); }

void
Grid_layout::set_geometry(Rect const &rect)
{
  if (!_d->need_refresh() && rect == geometry())
    return;

  Rect cr = /*alignment() ? aligned_rect(rect) : */ rect;
  int m = margin();
  Rect s = cr.grow(-m);
  _d->distribute(s, spacing());
  _geom = rect;
}

Rect
Grid_layout::cell_rect(int r, int c) const
{ return _d->cell_rect(r, c); }

void
Grid_layout::add_item(Layout_item *i)
{
  int r, c;
  _d->next_pos(&r, &c);
  add_item(i, r, c);
}

void
Grid_layout::add_item(Layout_item *i, int r, int c, int rs, int cs, Alignment a)
{
  G_box *b = new G_box(i);
  b->set_alignment(a);
  _d->add(b, r, rs < 0 ? -1 : r + rs - 1, c, (cs < 0) ? - 1 : c + cs - 1);
  i->set_parent_layout_item(this);
  invalidate();
}

void
Grid_layout::row_stretch(int r, int s)
{
  _d->set_row_stretch(r, s);
  invalidate();
}

int
Grid_layout::row_stretch(int r) const
{ return _d->row_stretch(r); }

void
Grid_layout::col_stretch(int c, int s)
{
  _d->set_col_stretch(c, s);
  invalidate();
}

int
Grid_layout::col_stretch(int c) const
{ return _d->col_stretch(c); }

void
Grid_layout::row_min_height(int r, int m)
{
  _d->set_row_spacing(r, m);
  invalidate();
}

int
Grid_layout::row_min_height(int r) const
{ return _d->row_spacing(r); }

void
Grid_layout::col_min_width(int c, int m)
{
  _d->set_col_spacing(c, m);
  invalidate();
}

int
Grid_layout::col_min_width(int c) const
{ return _d->col_spacing(c); }

Orientations
Grid_layout::expanding() const
{ return _d->expanding(spacing()); }

void
Grid_layout::invalidate()
{
  _d->invalidate();
  Layout::invalidate();
}

bool
Grid_layout::empty() const
{
  for (int i = 0; i < count(); ++i)
    if (!item(i)->empty())
      return false;

  return true;
}


}
