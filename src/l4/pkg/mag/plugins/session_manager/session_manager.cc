/*
 * (c) 2011 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/mag/server/session>
#include <l4/mag/server/plugin>
#include <l4/mag/server/view>
#include <l4/mag/server/user_state>
#include <l4/mag/server/menu>

#include <l4/mag-gfx/canvas>
#include <l4/mag-gfx/clip_guard>

#include <l4/re/event_enums.h>

namespace Mag_server { namespace {

typedef Menu<Session_list> Session_menu;

class Session_manager_view : public View, private cxx::Observer
{
private:
  Core_api const *_core;
  Session_menu _m;
  bool _visible;

  struct So : public Observer
  {
    explicit So(Session_manager_view *v) : v(v) {}
    Session_manager_view *v;
    void notify() { v->sessions_changed(); }
  };

  So _session_observer;

public:
  explicit Session_manager_view(Core_api const *core)
  : View(Rect(), View::F_super_view /*| F_transparent*/), _core(core),
    _m(core, this, core->sessions()),
    _visible(false), _session_observer(this)
  { }

  Observer *mode_observer() { return this; }
  Observer *session_observer() { return &_session_observer; }

  void handle_event(Hid_report *e, Point const &mouse, bool core_dev);
  void draw(Canvas *canvas, View_stack const *, Mode mode) const;
  void notify();
  void sessions_changed();
  void calc_geometry();
};

class Session_manager : public Plugin
{
private:
  Session_manager_view *_v;

public:
  Session_manager() : Plugin("Session manager") {}
  char const *type() const { return "Session manager"; }
  void start(Core_api *core);
  void stop();
};


void
Session_manager::start(Core_api *core)
{
  _v = new Session_manager_view(core);
  core->user_state()->vstack()->add_mode_observer(_v->mode_observer());
  core->add_session_observer(_v->session_observer());
}

void
Session_manager::stop()
{
  if (_v)
    delete _v;

  _v = 0;
}

static Session_manager session_manager;


void
Session_manager_view::handle_event(Hid_report *e, Point const &mouse, bool)
{
  Session_list::Const_iterator s = _m.handle_event(e, mouse - p1());

  if (s != _core->sessions()->end())
    {
      s->ignore(!s->ignore());
      _core->user_state()->vstack()->update_all_views();
    }
}

void
Session_manager_view::draw(Canvas *canvas, View_stack const *, Mode) const
{
  _m.draw(canvas, p1());
}

void
Session_manager_view::calc_geometry()
{
  enum {
    BORDER = 10,
  };
  Area cs = _core->user_state()->vstack()->canvas()->size();
  cs = cs - Area(BORDER * 2, BORDER * 2);
  Area sz =_m.calc_geometry(cs);

  Point p = Point(20, 20).min(Point(BORDER, BORDER) + Point(cs) - Point(sz));
  set_geometry(Rect(p, sz));
}

void
Session_manager_view::sessions_changed()
{
  if (!_visible)
    return;

  calc_geometry();
  _core->user_state()->vstack()->refresh_view(this, 0, *this);
}

void
Session_manager_view::notify()
{
  View_stack *vs = _core->user_state()->vstack();
  Mode m = vs->mode();
  if (_visible && !m.kill())
    {
      _visible = false;
      vs->remove(this);
      vs->refresh_view(0, 0, *this);
    }
  else if (!_visible && m.kill())
    {
      _visible = true;
      calc_geometry();
      vs->push_top(this, true);
    }
}

}}

