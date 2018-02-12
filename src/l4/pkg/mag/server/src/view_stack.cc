/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/mag-gfx/clip_guard>

#include "view_stack"
#include "view"
#include "session"

#include <cstdio>
#include <cstring>

namespace Mag_server {

class Redraw_queue
{
public:
  enum { Num_entries = 100 };

  // merge all overlapping rects in the queue
  void merge()
  {
    for (unsigned i = last; i > 0; --i)
      for (unsigned j = 0; j < last; ++j)
	{
	  if (i - 1 == j)
	    continue;

	  if ((_q[j] & _q[i-1]).valid())
	    {
	      _q[j] = _q[j] | _q[i-1];
	      memmove(_q + i - 1, _q + i, (last - i) * sizeof(Rect));
	      --last;
	      break;
	    }
	}
  }

  void q(Rect const &r)
  {
    if (!r.valid())
      return;
    //printf("q[%d,%d - %d,%d]\n", r.x1(), r.y1(), r.x2(), r.y2());
    for (unsigned i = 0; i < last; ++i)
      {
	if ((_q[i] & r).valid())
	  {
	    // merge with overlapping rect
	    _q[i] = _q[i] | r;
	    merge();
	    return;
	  }
      }

    if (last < Num_entries)
      _q[last++] = r; // add new entry
    else
      {
	// queue is full, just merge with the last entry
        _q[last-1] = _q[last - 1] | r;
	merge();
      }
  }

  void clear()
  { last = 0; }

  typedef Rect const *iterator;

  iterator begin() const { return _q; }
  iterator end() const { return _q + last; }

private:
  Rect _q[Num_entries];
  unsigned last;
};

static Redraw_queue rdq;

View const *
View_stack::next_view(View const *_v, View const *bg) const
{
  for (Const_view_iter v = _top.iter(_v); v != _top.end();)
    {
      ++v;
      if (v == _top.end())
	return 0;

      unsigned sf = 0;
      if (v->session())
        sf = v->session()->flags();

      if (sf & Session::F_ignore)
	continue;

      if (!v->background())
	return *v;

      if (v == _top.iter(_background) || v == _top.iter(bg))
	return *v;

      if (!bg && (sf & Session::F_default_background))
        return *v;
    }

  return 0;
}

Rect
View_stack::outline(View const *v) const
{
  if (_mode.flat() || !v->need_frame())
    return *v;
  else
    return v->grow(v->frame_width());
}

void
View_stack::viewport(View *v, Rect const &pos, bool) const
{
  Rect old = outline(v);

  /* take over new view parameters */
  v->set_geometry(pos);

  Rect compound = old | outline(v);

  /* update labels (except when moving the mouse cursor) */
  if (v != top())
    place_labels(compound);

  /* update area on screen */
  rdq.q(compound);
//  draw_recursive(top(), 0, /*redraw ? 0 : view->session(),*/ compound);
}

void
View_stack::draw_frame(View const *v) const
{
  if (_mode.flat() || !v->need_frame() || !v->session())
    return;

  Rgb32::Color color = v->session()->color();
  Rgb32::Color outline = v->focused() ? Rgb32::White : Rgb32::Black;

  int w = v->frame_width()-1;
  _canvas->draw_rect(v->offset(-1-w, -1-w, 1+w, 1+w), outline);
  _canvas->draw_rect(*v, color, -w);
}

static void
draw_string_outline(Canvas *c, Point const &pos, Font const *f, char const *txt)
{
  for (int i = -1; i <= 1; ++i)
    for (int k = -1; k <= 1; ++k)
      if (i || k)
        c->draw_string(pos + Point(i, k), f, Rgb32::Black, txt);
}

void
View_stack::draw_label(View const *v) const
{
  if (_mode.flat() || !v->need_frame())
    return;

  char const *const sl = v->session()->label();
  Point pos = v->label_pos() + Point(1, 1);
  draw_string_outline(_canvas, pos, _label_font, sl);
  _canvas->draw_string(pos, _label_font, Rgb32::White, sl);

  char const *const vl = v->title();
  if (!vl)
    return;

  pos = pos + Point(_label_font->str_w(sl) + View::Label_sep, 0);
  draw_string_outline(_canvas, pos, _label_font, vl);
  _canvas->draw_string(pos, _label_font, Rgb32::White, vl);
}

void
View_stack::set_focused(View *v)
{
  _focused = v;
  if (!v)
    return;

  // stack all 'above' tagged views of the focussed session
  // to the top. We keep their respective order.
  Session *s = v->session();
  if (!s)
    return;

  View_iter top = _top.iter(_no_stay_top);
  for (View_iter t = _top.begin(); t != _top.end(); ++t)
    {
      if (!t->above())
	continue;

      if (t->session() == s)
	{
	  stack(*t, *top, true);
	  top = t;
	}
    }

  // if the view is not a background than raise the view relative to
  // all views of other sessions, but keep the stacking order within
  // it's session
  if (v->background())
    return;

  for (top = --_top.iter(v);
       top != _top.iter(_no_stay_top) && top != _top.end() && top->session() != s;
       --top)
    ;

  if (top != --_top.iter(v))
    stack(v, *top, true);
}

void
View_stack::draw_recursive(View const *v, View const *dst, Rect const &rect) const
{ draw_recursive(v, dst, rect, current_background()); }

void
View_stack::draw_recursive(View const *v, View const *dst, Rect const &rect,
                           View const *bg) const
{
  Rect clipped;

  /* find next view that intersects with the current clipping rectangle */
  while (v && !(clipped = outline(v) & rect).valid())
    v = next_view(v, bg);

  if (!v)
    return;

  View const *n = next_view(v, bg);
  Rect_tuple border;

  if (v->transparent() && n)
    {
      draw_recursive(n, dst, rect, bg);
      n = 0;
    }
  else
    border = rect - clipped;

  if (n && border.t().valid())
    draw_recursive(n, dst, border.t(), bg);
  if (n && border.l().valid())
    draw_recursive(n, dst, border.l(), bg);

  if (!dst || dst == v || v->transparent())
    {
      Clip_guard g(_canvas, clipped);
      draw_frame(v);
      v->draw(_canvas, this, _mode);
      draw_label(v);
    }

  if (n && border.r().valid())
    draw_recursive(n, dst, border.r(), bg);
  if (n && border.b().valid())
    draw_recursive(n, dst, border.b(), bg);
}

void
View_stack::refresh_view(View const *v, View const *dst, Rect const &rect) const
{
  (void)dst;
  Rect r = rect;
  if (v)
    r = r & outline(v);

  rdq.q(r);
  //draw_recursive(top(), dst, r);
}

void
View_stack::flush()
{
  //static int cnt = 0;
  for (Redraw_queue::iterator i = rdq.begin(); i != rdq.end(); ++i)
    {
      //printf("redraw[%d] %d,%d-%d,%d\n", cnt++, i->x1(), i->y1(), i->x2(), i->y2());
      draw_recursive(top(), 0, *i);
      if (_canvas_view)
	_canvas_view->refresh(i->x1(), i->y1(), i->w(), i->h());
    }

  rdq.clear();
}

void
View_stack::stack(View *v, View *pivot, bool behind)
{
  if (_top.in_list(v))
    remove(v);

  if (behind)
    insert_after(v, pivot);
  else
    insert_before(v, pivot);

  place_labels(*v);

  refresh_view(v, 0, outline(v));
}

void
View_stack::push_bottom(View *v)
{
  Session *s = v->session();
  View *b = s ? s->background() : 0;
  stack(v, (b && (b != v)) ? b : _background, false);
}


View *
View_stack::find(Point const &pos) const
{
  View *bg = current_background();
  View *n = next_view(top(), bg);
  while (n && !n->contains(pos))
    n = next_view(n, bg);

  return n;
}

void
View_stack::optimize_label_rec(View *cv, View *lv, Rect const &rect, Rect *optimal,
                               View *bg) const
{
  /* if label already fits in optimized rectangle, we are happy */
  if (optimal->fits(lv->label_sz()))
    return;

  /* find next view that intersects with the rectangle or the target view */
  Rect clipped;
  while (cv && cv != lv && !(clipped = outline(cv) & rect).valid())
    cv = next_view(cv, bg);

  /* reached end of view stack */
  if (!cv)
    return;

  if (cv != lv && next_view(cv, bg))
    {
      /* cut current view from rectangle and go into sub rectangles */
      Rect r[4] =
	{ Rect(rect.p1(), Point(rect.x2(), clipped.y1() - 1)),
	  Rect(rect.p1(), Point(clipped.x1() - 1, rect.y2())),
	  Rect(Point(clipped.x2() + 1, rect.y1()), rect.p2()),
	  Rect(Point(rect.x1(), clipped.y2() + 1), rect.p2()) };
      for (int i = 0; i < 4; i++)
	optimize_label_rec(next_view(cv, bg), lv, r[i], optimal, bg);

      return;
    }

  /*
   * Now, cv equals lv and we must decide how to configure the
   * optimal rectangle.
   */

  /* stop if label does not fit vertically */
  if (rect.h() < lv->label_sz().h())
    return;

  /*
   * If label fits completely within current rectangle, we are done.
   * If label's width is not fully visible, choose the widest rectangle.
   */
  if (rect.fits(lv->label_sz()) || (rect.w() > optimal->w()))
    *optimal = rect;
}

void
View_stack::do_place_labels(Rect const &rect) const
{
  View *bg = current_background();
  View *start = next_view(*_top.begin(), bg);
  /* ignore mouse cursor */
  for (View *view = start; view && next_view(view); view = next_view(view, bg))
    if ((*view & rect).valid())
      {
	Rect old(view->label_pos(), view->label_sz());

	/* calculate best visible label position */
	Rect rect = Rect(Point(0, 0), _canvas->size()) & *view;
	Rect best;
	optimize_label_rec(start, view, rect, &best, bg);

	/*
	 * If label is not fully visible, we ensure to display the first
	 * (most important) part. Otherwise, we center the label horizontally.
	 */
	int x = best.x1();
	if (best.fits(view->label_sz()))
	  x += (best.w() - view->label_sz().w()) / 2;

	view->label_pos(Point(x, best.y1()));

	/* refresh old and new label positions */
	refresh_view(view, view, old);
	Rect n = Rect(view->label_pos(), view->label_sz());
	refresh_view(view, view, n);
      }
}

}
