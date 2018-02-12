/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/scrollbar>
#include <l4/scout-gfx/factory>

#include <algorithm>

#define  SLIDER_RGBA _binary_slider_rgba_start
#define UPARROW_RGBA _binary_uparrow_rgba_start
#define DNARROW_RGBA _binary_downarrow_rgba_start
#define LFARROW_RGBA _binary_leftarrow_rgba_start
#define RIARROW_RGBA _binary_rightarrow_rgba_start

extern unsigned char SLIDER_RGBA[];
extern unsigned char UPARROW_RGBA[];
extern unsigned char DNARROW_RGBA[];
extern unsigned char LFARROW_RGBA[];
extern unsigned char RIARROW_RGBA[];

namespace Scout_gfx {

/**
 * Event handler interface
 */
bool
Scrollbar::Arrow_event_handler::handle(Event const &ev)
{
  /* start movement with zero speed */
  if ((ev.type == Event::PRESS) && (ev.key_cnt == 1))
    {
      /* press icon (slight vertical shift, darker shadow) */
      _icon->rgba(_rgba, Area(32, 32), 1, 3);
      _icon->refresh();

      _curr_speed = _direction*256;
      _dst_speed  = _direction*Max_speed;
      _accel      = 16;
      _view_pos   = _sb->view_pos() << 8;
      schedule(10);
    }

  if ((ev.type == Event::RELEASE) && (ev.key_cnt == 0))
    {
      /* release icon */
      _icon->rgba(_rgba, Area(32, 32));
      _icon->refresh();

      _accel     = 64;
      _dst_speed = 0;
    }

  return true;
}

/**
 * Tick interface
 */
int
Scrollbar::Arrow_event_handler::on_tick()
{
  using std::min;
  using std::max;

  /* accelerate */
  if (_curr_speed < _dst_speed)
    _curr_speed = min(_curr_speed + _accel, _dst_speed);

  /* decelerate */
  if (_curr_speed > _dst_speed)
    _curr_speed = max(_curr_speed - _accel, _dst_speed);

  /* soft stopping on boundaries */
  while ((_curr_speed < 0) && (_view_pos > 0)
      && (_curr_speed*_curr_speed > _view_pos*_accel*4))
    _curr_speed = min(0, _curr_speed + _accel*4);

  int max_pos;
  while ((_curr_speed > 0)
      && ((max_pos = (_sb->real_size() - _sb->view_size())*256 - _view_pos) > 0)
      && (_curr_speed*_curr_speed > max_pos*_accel*4))
    _curr_speed = max(0, _curr_speed - _accel*4);

  /* move view position with current speed */
  _view_pos = max(0, _view_pos + _curr_speed);

  /* set new view position */
  int old_view_pos = _sb->view_pos();
  _sb->view(_sb->real_size(), _sb->view_size(), _view_pos>>8);
  if (old_view_pos != _sb->view_pos())
    _sb->notify_listener();

  /* keep ticking as long as we are on speed */
  return (_curr_speed != 0);
}


/**
 * Event handler interface
 */
bool
Scrollbar::Slider_event_handler::handle(Event const &ev)
{

  int p = _sb->orientation() == Vertical ? ev.m.y() : ev.m.x();
  /* start movement with zero speed */
  if ((ev.type == Event::PRESS) && (ev.key_cnt == 1))
    {
      /* press icon (slight vertical shift, darker shadow) */
      _icon->rgba(_rgba, Area(32,32), 1, 3);
      _icon->refresh();

      _om = p;
      _cm = p;
      _op = _sb->slider_pos();
    }

  if ((ev.type == Event::RELEASE) && (ev.key_cnt == 0))
    {
      /* release icon */
      _icon->rgba(_rgba, Area(32, 32));
      _icon->refresh();
    }

  if (ev.key_cnt && (p != _cm))
    {
      _cm = p;
      _sb->slider_pos(_op + _cm - _om);
      _sb->notify_listener();
    }

  return true;
}



/*************************
 ** Scrollbar interface **
 *************************/

Scrollbar::Scrollbar(Factory *f, Orientation o)
: _uparrow(f->create_icon(o == Vertical ? UPARROW_RGBA : LFARROW_RGBA, Area(32, 32))),
  _dnarrow(f->create_icon(o == Vertical ? DNARROW_RGBA : RIARROW_RGBA, Area(32, 32))),
  _slider(f->create_icon(SLIDER_RGBA, Area(32, 32))),
  _orientation(o),
  _uph(this, _uparrow, -1, o == Vertical ? UPARROW_RGBA : LFARROW_RGBA),
  _dnh(this, _dnarrow,  1, o == Vertical ? DNARROW_RGBA : RIARROW_RGBA),
  _slh(this, _slider, SLIDER_RGBA)

{
  /* init scrollbar elements */
  _uparrow->alpha(0);
  _dnarrow->alpha(0);
  _slider->alpha(0);

  append(_uparrow);
  append(_dnarrow);
  append(_slider);

  /* define event handlers for scrollbar elements */
  _uparrow->event_handler(&_uph);
  _dnarrow->event_handler(&_dnh);
  _slider->event_handler(&_slh);

  if (o == Vertical)
    _size = Area(Sb_elem_w, Sb_elem_h*3);
  else
    _size = Area(Sb_elem_w * 3, Sb_elem_h);

  _real_size  = 100;
  _view_size  = 100;
  _view_pos   = 0;
  _listener   = 0;
  _visibility = 0;
}

void
Scrollbar::refresh_slider_geometry()
{
  if (_orientation == Vertical)
    _slider->set_geometry(Rect(Point(0, slider_pos()), Area(Sb_elem_w, slider_size())));
  else
    _slider->set_geometry(Rect(Point(slider_pos(), 0), Area(slider_size(), Sb_elem_h)));
}

void
Scrollbar::slider_pos(int pos)
{
  int slider_bg_h;
  int n;
  if (_orientation == Vertical)
    {
      n = Sb_elem_h;
      slider_bg_h = _size.h() - Sb_elem_h*2;
    }
  else
    {
      n = Sb_elem_w;
      slider_bg_h = _size.w() - Sb_elem_w*2;
    }

  _view_pos = ((pos - n)*_real_size)/slider_bg_h;
  _view_pos = std::max(0, std::min(_view_pos,  _real_size - _view_size));

  refresh_slider_geometry();

}



/***********************
 ** Element interface **
 ***********************/

Area
Scrollbar::min_size() const
{
  int a = Sb_elem_w * (_orientation == Horizontal ? 4 : 1);
  int b = Sb_elem_h * (_orientation == Vertical ? 4 : 1);
  return Area(a, b);
}

Area
Scrollbar::preferred_size() const
{ return Scrollbar::min_size(); }

Area
Scrollbar::max_size() const
{
  if (_orientation == Vertical)
    return Area(preferred_size().w(), Area::Max_h);
  else
    return Area(Area::Max_w, preferred_size().h());
}

void
Scrollbar::set_geometry(Rect const &r)
{
  Area s = r.area();
  s = s.min(max_size()).max(min_size());
  _pos = r.p1();
  _size = s;

  if (_orientation == Vertical)
    _dnarrow->set_geometry(Rect(Point(0, s.h() - Sb_elem_h), _dnarrow->size()));
  else
    _dnarrow->set_geometry(Rect(Point(s.w() - Sb_elem_w, 0), _dnarrow->size()));

  refresh_slider_geometry();
}

void
Scrollbar::visibility(bool new_visibility)
{
  if (_visibility ^ new_visibility)
    {
      enum { Default_alpha = 100 };
      int alpha = new_visibility ? Default_alpha : 0;
      int speed = new_visibility ? 3 : 2;
      dynamic_cast<Fader*>(_uparrow)->fade_to(alpha, speed);
      dynamic_cast<Fader*>(_dnarrow)->fade_to(alpha, speed);
      dynamic_cast<Fader*>(_slider)->fade_to(alpha, speed);
    }

  _visibility = new_visibility;
}


}
