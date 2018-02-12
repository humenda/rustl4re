/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/widget>
#include <cstdio>


namespace Scout_gfx {

Widget::~Widget()
{
  if (_parent)
    _parent->forget(this);
}

void
Parent_widget::draw(Canvas *c, Point const &p)
{
  for (Widget *e = _first; e; e = e->next)
    e->try_draw(c, e->pos() + p);
}

void Parent_widget::append(Widget *e)
{
  if (_last)
    _last->next = e;
  else
    _first = e;

  _last = e;

  e->parent(this);
}


void Parent_widget::remove(Widget *e)
{
  if (_first == e)
    {
      _first = e->next;
      if (_last == e)
	_last = 0;

      return;
    }

  /* search specified element in the list */
  Widget *ce = _first;
  for (; ce && ce->next != e; ce = ce->next)
    ;

  if (!ce)
    return;

  if (_last == e)
    _last = ce;

  ce->next = e->next;
}


void Parent_widget::forget(Widget *e)
{
  if (e->parent() == this)
    remove(e);

  if(_parent)
    _parent->forget(e);
}



Widget *
Parent_widget::find_child(Point const &p)
{
  /* check if position is outside the parent element */
  if (!geometry().contains(p))
    return 0;

  Point np = p - _pos;

  /* check children */
  for (Widget *e = _first; e; e = e->next)
    {
      if (e->geometry().contains(np) && e->findable())
	return e;
    }

  return 0;
}

Widget *
Widget::find(Point const &p)
{
  if (geometry().contains(p) && _flags.findable)
    return this;

  return 0;
}


Widget *
Parent_widget::find(Point const &p)
{
  if (!findable())
    return 0;

  /* check if position is outside the parent element */
  if (!geometry().contains(p))
    return 0;

  Point np = p - _pos;

  /* check children */
  Widget *ret = this;
  for (Widget *e = _first; e; e = e->next)
    {
      Widget *res  = e->find(np);
      if (res)
        ret = res;
    }

  return ret;
}

Widget *
Widget::root()
{
  if (!_parent || this == _parent)
    return this;

  return _parent->root();
}

Widget *
Widget::next_widget(bool)
{
  if (!next)
    //backtrack:
    return _parent->next_widget(true);

  return next;
}

Widget *
Parent_widget::next_widget(bool bt)
{
  if (0) printf("Parent_widget::next()\n");
  if (bt || !_first) //just used as flag
    {
      if (next)
        {
          if (0) printf("subtree searched -> next\n");
          return next;
        }
      else
        {
          if (0) printf("backtracking\n");
          if (!_parent || _parent == this)
            return 0; //root: may be _null

          return _parent->next_widget(1);
        }
    }
  if (_first)
    {
      if (0) printf("descending\n");
      return _first;
    }
  if (0) printf("@return\n");
  return 0; //this should not happen - empty root
}

void
Widget::redraw_area(Rect const &r) const
{
  /* intersect specified area with element geometry */
  Rect n = (r & Rect(_size));

  if (!n.valid())
    return;

  /* propagate redraw request to the parent */
  if (_parent)
    _parent->redraw_area(n + _pos);
}

}
