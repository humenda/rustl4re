/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "client_fb.h"

#include <l4/mag-gfx/clip_guard>
#include <l4/mag-gfx/texture>

#include <l4/mag/server/view_stack>
#include <l4/mag/server/factory>

#include <l4/re/env>
#include <l4/re/event_enums.h>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/unique_cap>
#include <l4/sys/factory>
#include <l4/re/util/meta>
#include <l4/re/console>
#include <l4/re/event-sys.h>

#include <l4/mag/server/user_state>
#include <cstdio>
#include <cstring>


namespace Mag_server {

using L4Re::chksys;
using L4Re::chkcap;

enum { Bar_height = 16 };

Client_fb::Client_fb(Core_api const *core)
: View(Rect(), F_need_frame),
  Icu_svr(1, &_ev_irq),
  _core(core), _fb(0),
  _bar_height(Bar_height),
  _flags(0)
{}

void
Client_fb::set_geometry_prop(Session *_s, Property_handler const *, cxx::String const &v)
{
  Client_fb *s = static_cast<Client_fb*>(_s);
  // ignore multiple geometry properties
  if (s->_fb)
    return;

  int w, h, x=50, y=50;
  int r;

  cxx::String a = v;

  r = a.from_dec(&w);
  if (r >= a.len() || a[r] != 'x')
    L4Re::chksys(-L4_EINVAL, "invalid geometry format");

  a = a.substr(r + 1);
  r = a.from_dec(&h);

  if (r < a.len() && a[r] == '+')
    {
      a = a.substr(r + 1);
      r = a.from_dec(&x);
    }

  if (r < a.len() && a[r] == '+')
    {
      a = a.substr(r + 1);
      r = a.from_dec(&y);
    }

  if (w <= 0 || h <= 0 || w >= 10000 || h >= 10000)
    L4Re::chksys(-L4_ERANGE, "invalid geometry (too big)");

  Area sz = s->_core->user_state()->vstack()->canvas()->size();

  if (x < 10 - w)
    x = 10 - w;

  if (x >= sz.w())
    x = sz.w() - 10;

  if (y < 10 - h)
    y = 10 - h;

  if (y >= sz.h())
    y = sz.h() - 10;

  s->set_geometry(Rect(Point(x, y), Area(w, h)));
}

void
Client_fb::set_flags_prop(Session *_s, Property_handler const *p, cxx::String const &)
{
  Client_fb *s = static_cast<Client_fb*>(_s);

  if (!strcmp(p->tag, "focus"))
    s->_flags |= F_fb_focus;

  if (!strcmp(p->tag, "shaded"))
    s->_flags |= F_fb_shaded;

  if (!strcmp(p->tag, "fixed"))
    s->_flags |= F_fb_fixed_location;
}

void
Client_fb::set_bar_height_prop(Session *_s, Property_handler const *, cxx::String const &v)
{
  Client_fb *s = static_cast<Client_fb*>(_s);
  int r = v.from_dec(&s->_bar_height);
  if (r < v.len())
    L4Re::chksys(-L4_EINVAL, "invalid bar height format");

  s->_bar_height = std::max(std::min(s->_bar_height, 100), 4);
}

int
Client_fb::setup()
{
  using L4Re::Video::View;
  using L4Re::Video::Color_component;
  using L4Re::Video::Goos;

  Area res(size());

  auto ds = L4Re::Util::make_unique_cap<L4Re::Dataspace>();

  Screen_factory *sf = dynamic_cast<Screen_factory*>(_core->user_state()->vstack()->canvas()->type()->factory);
  //Screen_factory *sf = dynamic_cast<Screen_factory*>(Rgb16::type()->factory);

  L4Re::chksys(L4Re::Env::env()->mem_alloc()->alloc(sf->get_texture_size(res),
                                                    ds.get()));

  L4Re::Rm::Unique_region<void *> dsa;
  L4Re::chksys(L4Re::Env::env()->rm()
                 ->attach(&dsa, ds->size(), L4Re::Rm::Search_addr,
                          L4::Ipc::make_cap_rw(ds.get()), 0,
                          L4_SUPERPAGESHIFT));

  _fb = sf->create_texture(res, dsa.get());

  set_geometry(Rect(p1(), visible_size()));
  dsa.release();
  _fb_ds = ds.release();

  _view_info.flags = View::F_none;

  _view_info.view_index = 0;
  _view_info.xpos = 0;
  _view_info.ypos = 0;
  _view_info.width = _fb->size().w();
  _view_info.height = _fb->size().h();
  _view_info.buffer_offset = 0;
  _view_info.buffer_index = 0;
  _view_info.bytes_per_line = _view_info.width * _fb->type()->bytes_per_pixel();
  _view_info.pixel_info = *_fb->type();

  _screen_info.flags = Goos::F_pointer;
  _screen_info.width = _view_info.width;
  _screen_info.height = _view_info.height;
  _screen_info.num_static_views = 1;
  _screen_info.num_static_buffers = 1;
  _screen_info.pixel_info = _view_info.pixel_info;


  L4Re::Env const *e = L4Re::Env::env();
  _ev_ds = L4Re::Util::make_unique_cap<L4Re::Dataspace>();


  chksys(e->mem_alloc()->alloc(L4_PAGESIZE, _ev_ds.get()));
  chksys(e->rm()->attach(&_ev_ds_m, L4_PAGESIZE, L4Re::Rm::Search_addr,
                         L4::Ipc::make_cap_rw(_ev_ds.get()), 0, L4_PAGESHIFT));

  _events = L4Re::Event_buffer(_ev_ds_m.get(), L4_PAGESIZE);

  calc_label_sz(_core->label_font());
  return 0;
}

void
Client_fb::view_setup()
{
  if (_flags & F_fb_focus)
    _core->user_state()->set_focus(this);
}

void
Client_fb::draw(Canvas *canvas, View_stack const *, Mode mode) const
{
  /* use dimming in x-ray mode */
  Canvas::Mix_mode op = mode.flat() ? Canvas::Solid : Canvas::Mixed;

  /* do not dim the focused view in x-ray mode */
  if (mode.xray() && !mode.kill() && focused())
    op = Canvas::Solid;

  /*
   * The view content and label should never overdraw the
   * frame of the view in non-flat Nitpicker modes. The frame
   * is located outside the view area. By shrinking the
   * clipping area to the view area, we protect the frame.
   */
  Clip_guard clip_guard(canvas, *this);

  /*
   * If the clipping area shrinked to zero,
   * we do not process drawing operations.
   */
  if (!canvas->clip_valid()/* || !_session*/)
    return;

  /* draw view content */
  Rgb32::Color mix_color = /*mode.kill() ? kill_color() :*/ session()->color();

  canvas->draw_box(top(_bar_height), Rgb32::Color(56, 68, 88));

  canvas->draw_texture(_fb, mix_color, p1() + Point(0, _bar_height), op);
}

Area
Client_fb::visible_size() const
{
  if (_flags & F_fb_shaded)
    return Area(_fb->size().w(), _bar_height);

  return _fb->size() + Area(0, _bar_height);
}


void
Client_fb::toggle_shaded()
{
  Rect r = *this;
  _flags ^= F_fb_shaded;
  Rect n = Rect(p1(), visible_size());
  set_geometry(n);
  _core->user_state()->vstack()->refresh_view(0, 0, r | n);
}

bool
Client_fb::handle_core_event(Hid_report *e, Point const &mouse)
{
  static Point left_drag;
  static l4_umword_t drag_dev;
  bool consumed = false;

  Valuator<int> const *abs = e->get_vals(3);
  if (abs && abs->get(1).valid() && left_drag != Point())
    {
      Rect npos = Rect(p1() + mouse - left_drag, size());
      left_drag = mouse;
      _core->user_state()->vstack()->viewport(this, npos, true);
      consumed = true;
    }

  Rect bar = top(_bar_height);

  Hid_report::Key_event const *btn_left = e->find_key_event(L4RE_BTN_LEFT);
  View_stack *_stack = _core->user_state()->vstack();
  if (btn_left && btn_left->value == 1 && !_stack->on_top(this))
    _stack->push_top(this);

  if (btn_left && bar.contains(mouse) && !(_flags & F_fb_fixed_location))
    {
      if (btn_left->value == 1 && left_drag == Point())
	{
	  left_drag = mouse;
	  drag_dev = e->device_id();
	}
      else if (btn_left->value == 0 && e->device_id() == drag_dev)
	left_drag = Point();
      consumed = true;
    }

  Hid_report::Key_event const *btn_middle = e->find_key_event(L4RE_BTN_MIDDLE);
  if (btn_middle && btn_middle->value == 1 && bar.contains(mouse))
    {
      toggle_shaded();
      consumed = true;
    }

  // no events if window is shaded
  if (consumed || (_flags & F_fb_shaded))
    return true;

  return false;
}

namespace {
  struct Abs_xfrm
  {
    Point p1;
    Area sz;
    Axis_info_vector const *v;

    Abs_xfrm(Axis_info_vector const *v, Point const &p1, Area const &sz)
    : p1(p1), sz(sz), v(v) {}

    void operator () (unsigned axis, int &val) const
    {
      Axis_info const *ai = v->get(axis);
      if (ai)
	{
	  switch (ai->mode)
	    {
	    case 1:
	      val = cxx::min(sz.w(), cxx::max(0, val - p1.x()));
	      break;
	    case 2:
	      val = cxx::min(sz.h(), cxx::max(0, val - p1.y()));
	      break;
	    default:
	      break;
	    }
	}
    }
  };
}

void
Client_fb::handle_event(Hid_report *e, Point const &mouse, bool core_dev)
{
  static Point left_drag;

  if (core_dev && handle_core_event(e, mouse))
    return;

  // no events if window is shaded
  if (_flags & F_fb_shaded)
    return;

  Abs_xfrm xfrm(e->abs_infos(), p1() + Point(0, _bar_height), _fb->size());
  bool trigger = post_hid_report(e, _events, xfrm);

  if (trigger)
    _ev_irq.trigger();
}

void
Client_fb::put_event(l4_umword_t stream, int type, int code, int value,
                     l4_uint64_t time)
{
  L4Re::Event_buffer::Event e;
  e.time = time;
  e.payload.stream_id = stream;
  e.payload.type = type;
  e.payload.code = code;
  e.payload.value = value;
  _events.put(e);
  _ev_irq.trigger();
}

int
Client_fb::refresh(int x, int y, int w, int h)
{
  _core->user_state()->vstack()->refresh_view(this, 0, Rect(p1() + Point(x, y + _bar_height), Area(w, h)));
  return 0;
}

int
Client_fb::get_stream_info_for_id(l4_umword_t id, L4Re::Event_stream_info *info)
{
  return _core->user_state()->get_input_stream_info_for_id(id, info);
}

int
Client_fb::get_abs_info(l4_umword_t id, unsigned naxes, unsigned const *axes,
                        L4Re::Event_absinfo *infos)
{
  unsigned char ax_mode[naxes];
  int i = _core->user_state()->get_input_axis_info(id, naxes, axes, infos, ax_mode);
  //  < 0: means we got an error
  // == 0: means the device is an non-core device and not translated
  // == 1: means ths device is used as core input pointer (adjust X and Y)
  if (i < 0)
    return i;

  for (unsigned a = 0; a < naxes; ++a)
    {
      switch (ax_mode[a])
	{
	default: break;
	case 0: break;
	case 1:
		infos[a].min = 0;
		infos[a].max = _fb->size().w();
		break;
	case 2:
		infos[a].min = 0;
		infos[a].max = _fb->size().h();
		break;
	}
    }
  return i;
}




void
Client_fb::destroy()
{
  _core->user_state()->forget_view(this);
  delete _fb;
  _fb = 0;
}

}
