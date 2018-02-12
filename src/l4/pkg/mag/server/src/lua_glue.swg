// vi:ft=cpp
/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

%module Mag

%include <typemaps.i>
%include <lua_typemap.i>

/* typemap for our input values (supporting and invalid value as 'nil') */
%typemap(out) Mag_server::Value<int>
%{  if ($1.valid()) lua_pushinteger(L, $1.val()); else lua_pushnil(L); SWIG_arg++; %}

%typemap(in) Mag_server::Value<int>
%{
  if (lua_isnil(L, $input))
    $1 = Mag_server::Value<int>();
  else 
    {
      SWIG_contract_assert(lua_isinteger(L, $input), "argument must be a number")
      $1 = Mag_server::Value<int>(lua_tointeger(L, $input));
    }
%}

/* we report key events as pair of code and value in lua */
%typemap(out) Mag_server::Hid_report::Key_event const *
%{
    if ($1)
      {
        lua_pushinteger(L, $1->code);
        lua_pushinteger(L, $1->value);
        SWIG_arg += 2;
      }
%}


%{
#include <l4/mag/server/plugin>
#include <l4/mag/server/hid_report>
#include <l4/mag/server/valuator>
#include <l4/mag/server/user_state>
#include <l4/mag/server/input_source>
#include <l4/mag/server/session>
#include "axis_buf.h"

namespace Mag_server {
extern Mag_server::User_state *user_state;
extern Core_api *core_api;
}

int luaopen_Mag(lua_State*);

static int lua_session_iterator(lua_State *L)
{
  using Mag_server::Session;

  int b = lua_upvalueindex(1);
  int e = lua_upvalueindex(2);
  Session *bs, *es;
  if (!lua_isuserdata(L, b) || !lua_isuserdata(L, e)
      || SWIG_ConvertPtr(L, b, (void**)&bs, SWIGTYPE_p_Mag_server__Session, 0) == -1
      || SWIG_ConvertPtr(L, e, (void**)&es, SWIGTYPE_p_Mag_server__Session, 0) == -1)
    return 0;

  if (bs == es)
    return 0;

  lua_pushvalue(L, b);
  bs = static_cast<Session*>(cxx::D_list_item_policy::next(bs));
  SWIG_NewPointerObj(L, bs, SWIGTYPE_p_Mag_server__Session, 0);
  lua_replace(L, b);
  return 1;
}

%}

%ignore Mag_server::View_proxy::view;
%ignore Mag_server::View_proxy::forget;
%ignore Mag_server::View;

%ignore Mag_server::User_state::User_state;
%ignore Mag_server::User_state::forget_view;
%ignore Mag_server::Mode;
%ignore L4Re::Event;
%ignore L4Re::Default_event_payload;
%ignore L4Re::Event_buffer_t;
%ignore L4Re::Event_buffer;

#pragma SWIG nowarn=401,325

#define L4_EXPORT
#define L4_KOBJECT(x)

%nodefaultctor;
%nodefaultdtor;

%ignore Mag_server::Session::Property_handler;
%ignore cxx::String;
%ignore Mag_server::Session_list;
%ignore cxx::D_list<Mag_server::Session>;
%ignore Mag_server::Session::set_label_prop;
%ignore Mag_server::Session::set_color_prop;
%ignore Mag_server::Session::color;
%ignore Mag_server::Session::set_notifier;
%ignore Mag_server::Session::background;

%include <server/session>

%clearnodefaultctor;
%clearnodefaultdtor;

%include <server/mode>
%include <server/valuator>

%apply unsigned *OUTPUT { unsigned *bus, unsigned *vendor, unsigned *product, unsigned *version };
#define L4_RPC(a, b, c...)
#define L4_RPC_NF(a, b, c...)
%include <l4re-core/l4re/include/event>
%clear unsigned *bus, unsigned *vendor, unsigned *product, unsigned *version;
%include <axis_buf.h>


%template(Valuatori) Mag_server::Valuator<int>;
%template(Valuei) Mag_server::Value<int>;

namespace Mag_server {

class Axis_info
{
public:
  int value;
  int min;
  int max;
  int fuzz;
  int flat;
  int resolution;
  int delta;
  int mode;
};

class Axis_info_vector
{
public:
  Axis_info_vector();
  explicit Axis_info_vector(unsigned size);
  ~Axis_info_vector();

  unsigned size() const;
  Axis_info const *get(unsigned idx) const;
  Axis_info *get(unsigned idx);
  Axis_info *create(unsigned idx);
  %apply SWIGTYPE *DISOWN { Axis_info *info };
  bool set(unsigned idx, Axis_info *info);
  %clear Axis_info *info;
};

%extend Axis_info_vector
{
  Mag_server::Axis_info *__getitem__(unsigned idx)
  {
    return self->get(idx);
  }
};

class Hid_report
{
public:
  struct Key_event;
  Hid_report(l4_umword_t device_id, unsigned rels, unsigned abss, unsigned mscs,
             unsigned sws, unsigned mts);

  ~Hid_report();

  bool get(unsigned char type, unsigned code, int &val) const;
  void set(unsigned char type, unsigned code, int val);

  bool mt_get(unsigned id, unsigned code, int &val) const;

  void mt_set(unsigned code, int val);
  bool submit_mt();

  Valuator<int> const *get_vals(unsigned char type) const;
  Valuator<int> *get_vals(unsigned char type);
  Valuator<int> const *get_mt_vals(unsigned id) const;

  bool add_key(int code, int value);
  Key_event const *get_key_event(unsigned idx) const;
  Key_event const *find_key_event(int code) const;
  void remove_key_event(int code);

  void sync(long long time);
  long long time() const;
  void clear();
  l4_umword_t device_id() const;

  Axis_info_vector const *abs_infos() const;
  Axis_info_vector *abs_infos();
  void set_abs_info(Axis_info_vector *i);
};

}

%extend Mag_server::Valuator<int>
{
  Mag_server::Value<int> __getitem__(unsigned idx) const
  {
    return self->get(idx);
  }

  void __setitem__(unsigned idx, Mag_server::Value<int> v)
  {
    if (v.valid())
      self->set(idx, v.val());
    else
      self->inv(idx);
  }
}

%extend Mag_server::Hid_report
{
  Mag_server::Valuator<int> *__getitem__(unsigned char type)
  {
    return self->get_vals(type);
  }
};

%nodefaultctor;
class Mag_server::User_state
{
public:
  void set_pointer(int x, int y, bool hide);
};

class Mag_server::View_proxy
{
public:
  View_proxy(Mag_server::User_state *ust);
};

%clearnodefaultctor;

%extend Mag_server::User_state
{
  bool set_kbd_focus(Mag_server::View_proxy *vp)
  {
    if (!vp)
      return self->set_focus(0);
    else
      return self->set_focus(vp->view());
  }

  void post_event(Mag_server::View_proxy *vp, Mag_server::Hid_report *e, bool update,
                  bool core_ev)
  {
    Mag_server::View *v = vp ? vp->view() : self->kbd_focus();
    if (update)
      self->vstack()->update_all_views();

    //maybe_screenshot(ust, e);

    if (v && (!self->vstack()->mode().kill() || v->super_view()))
      v->handle_event(e, self->mouse_pos(), core_ev);
  }

  void toggle_mode(Mag_server::Mode::Mode_flag mode)
  {
    self->vstack()->toggle_mode(mode);
  }

  void find_pointed_view(Mag_server::View_proxy *vp)
  {
    if (!vp)
      return;

    vp->view(self->vstack()->find(self->mouse_pos()));
  }

  %apply int *OUTPUT {int *w, int *h};
  void screen_size(int *w, int *h)
  {
    Mag_gfx::Area s = self->vstack()->canvas()->size();
    *w = s.w();
    *h = s.h();
  }
};

%extend Mag_server::View_proxy
{
  void set(Mag_server::View_proxy *src)
  {
    self->view(src ? src->view() : 0);
  }
};

%nodefaultctor;

%typemap(in) l4_umword_t USERDATA;
%typemap(in,checkfn="lua_isuserdata") l4_umword_t USERDATA
%{ $1 = (l4_umword_t)lua_touserdata(L, $input); %}


class Mag_server::Input_source
{};

%extend Mag_server::Input_source
{
public:
  int get_stream_info(l4_umword_t id, L4Re::Event_stream_info **OUTPUT)
  {
    *OUTPUT = new L4Re::Event_stream_info();
    return self->get_stream_info(id, *OUTPUT);
  }

  int get_abs_info(l4_umword_t id, unsigned axis, Mag_server::Axis_info **OUTPUT)
  {
    *OUTPUT = 0;
    unsigned axes[1];
    axes[0] = axis;
    L4Re::Event_absinfo info[1];
    int res = self->get_axis_info(id, 1, axes, info);
    if (res < 0)
      return res;

    Mag_server::Axis_info *i = new Mag_server::Axis_info();
    *OUTPUT = i;

    i->value = info->value;
    i->min = info->min;
    i->max = info->max;
    i->fuzz = info->fuzz;
    i->flat = info->flat;
    i->resolution = info->resolution;
    i->delta = info->max - info->min;
    i->mode = 0;
    return res;
  }
};


namespace Mag_server {
%typemap(out) Session_list * {
    SWIG_NewPointerObj(L, *($1->begin()), SWIGTYPE_p_Mag_server__Session, 0);
    SWIG_NewPointerObj(L, *($1->end()), SWIGTYPE_p_Mag_server__Session, 0);
    lua_pushcclosure(L, &lua_session_iterator, 2);
    return 1;
}

%nodefaultctor Core_api;
%nodefaultdtor Core_api;
class Core_api
{
public:
  Session_list *sessions() const;
};

%immutable;
extern Core_api *core_api;
%mutable;

}
%clearnodefaultctor;

%extend L4Re::Event_stream_info
{
public:
  void get_device_id(unsigned *bus, unsigned *vendor, unsigned *product, unsigned *version)
  {
    *bus = self->id.bustype;
    *vendor = self->id.vendor;
    *product = self->id.product;
    *version = self->id.version;
  }
};

%inline %{
static Mag_server::User_state *get_user_state()
{
  return Mag_server::user_state;
}
%}

