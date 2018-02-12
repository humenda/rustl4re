/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "input_source"
#include "user_state"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <l4/cxx/exceptions>
#include <l4/re/event_enums.h>
#include <errno.h>
#include <cstring>

#include "lua_glue.swg.h"

//#define CONFIG_SCREENSHOT
#if defined (CONFIG_SCREENSHOT)
# include <zlib.h>


namespace {

using namespace Mag_server;

static void base64_encodeblock(char const *_in, char *out)
{
  unsigned char const *in = (unsigned char const *)_in;

  out[0] = (in[0] >> 2) + ' ';
  out[1] = (((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)) + ' ';
  out[2] = (((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)) + ' ';
  out[3] = (in[2] & 0x3f) + ' ';
}

static void uuencode(char const *in, unsigned long bytes)
{
  char uubuf[100];
  char *out;

  while (bytes)
    {
      unsigned cnt = 0;
      out = uubuf;

      while (bytes)
	{
	  unsigned n = 3;
	  if (bytes < n)
	    n = bytes;

	  if (cnt + n > 45)
	    break;

	  base64_encodeblock(in, out);
	  in += 3;
	  out += 4;

	  bytes -= n;
	  cnt += n;
	}
      *out = 0;

      printf("%c%s\n", cnt + ' ', uubuf);
    }
}

static void maybe_screenshot(User_state *ust, User_state::Event const &e)
{
  if (e.payload.type != L4RE_EV_KEY || e.payload.value != 1
      || e.payload.code != L4RE_KEY_F12)
    return;

  char obuf[1024];
  int flush;
  printf("Start screen shot ==========================\n");
  printf("begin 600 screenshot.Z\n");

  z_stream s;
  s.zalloc = Z_NULL;
  s.zfree = Z_NULL;
  s.opaque = Z_NULL;
  s.next_in = (Bytef*)ust->vstack()->canvas()->buffer();
  s.avail_in = ust->vstack()->canvas()->size().h() * ust->vstack()->canvas()->bytes_per_line();
  s.total_in = s.avail_in;
  s.total_out = 0;

  deflateInit2(&s, 9, Z_DEFLATED, 16 + 15, 9, Z_DEFAULT_STRATEGY);

  do
    {
      flush = s.avail_in ? Z_NO_FLUSH : Z_FINISH;
      do
	{
	  s.next_out = (Bytef*)obuf;
	  s.avail_out = sizeof(obuf);

	  if (deflate(&s, flush) == Z_STREAM_ERROR)
	    {
	      printf("\nERROR: compressing screenshot\n");
	      break;
	    }

	  unsigned b = sizeof(obuf) - s.avail_out;
	  if (b)
	    uuencode(obuf, b);
	}
      while (s.avail_out == 0);
    }
  while (flush != Z_FINISH); //s.avail_out == 0);

  deflateEnd(&s);
  printf(" \nend\n");
  printf("\nEND screen shot ==========================\n");
}

}
#else
inline void maybe_screenshot(Mag_server::User_state *, Mag_server::User_state::Event const &) {}
#endif

int luaopen_Mag(lua_State*);

namespace Mag_server {

User_state *user_state;

Hid_report::Hid_report(l4_umword_t dev_id, unsigned rels, unsigned abss, unsigned mscs,
                       unsigned sws, unsigned mts)
: _device_id(dev_id), _kevs(0), _abs_info(0)
{
  _vals[0].create(rels, 0);
  _vals[1].create(abss, 0);
  _vals[2].create(mscs, 0);
  _vals[3].create(sws, 0);

  _mts = 0;
  if (mts)
    {
      _mts = mts + 1;
      _mt = new Valuator<int>[_mts];
    }

  for (unsigned i = 0; i < _mts; ++i)
    _mt[i].create(Num_mt_vals, Mt_val_offset); // 13 mt axes, currently
}

Hid_report::~Hid_report()
{
  if (_mts)
    delete [] _mt;
}

void
Hid_report::clear()
{
  for (unsigned i = 0; i < Num_types; ++i)
    _vals[i].clear();

  for (unsigned i = 0; i < _mts; ++i)
    _mt[i].clear();

  _kevs = 0;
}

Valuator<int> const *
Hid_report::get_vals(unsigned char type) const
{
  // check for out of range...
  if (type < Type_offset || (type = type - Type_offset) >= Num_types)
    return 0;

  return &_vals[type];
}

Valuator<int> *
Hid_report::get_vals(unsigned char type)
{
  // check for out of range...
  if (type < Type_offset || (type = type - Type_offset) >= Num_types)
    return 0;

  return &_vals[type];
}

Valuator<int> const *
Hid_report::get_mt_vals(unsigned id) const
{
  if (_mts && id < _mts - 1)
    return &_mt[id];

  return 0;
}

bool
Hid_report::get(unsigned char type, unsigned code, int &val) const
{
  // check for out of range...
  if (type < Type_offset || (type = type - Type_offset) >= Num_types)
    return false;

  if (!_vals[type].valid(code))
    return false;

  val = _vals[type][code].val();
  return true;
}

void
Hid_report::set(unsigned char type, unsigned code, int val)
{
  // check for out of range...
  if (type < Type_offset || (type = type - Type_offset) >= Num_types)
    return;

  _vals[type].set(code, val);
}

bool
Hid_report::mt_get(unsigned id, unsigned code, int &val) const
{
  if (id >= _mts)
    return false;

  if (!_mt[id].valid(code))
    return false;

  val = _mt[id][code].val();
  return true;
}

void
Hid_report::mt_set(unsigned code, int val)
{
  if (!_mts)
    return;

  _mt[_mts-1].set(code, val);
}

bool
Hid_report::submit_mt()
{
  if (!_mts)
    return false;

  Valuator<int> &s = _mt[_mts-1];
  Value<int> id = s[0x39];
  if (!id.valid())
    {
      s.clear();
      return false;
    }

  if (id.val() >= (long)_mts - 1)
    {
      s.clear();
      return false;
    }

  _mt[id.val()].swap(s);
  return true;
}

bool
Hid_report::add_key(int code, int value)
{
  if (_kevs >= Num_key_events)
    return false;

  _kev[_kevs].code = code;
  _kev[_kevs].value = value;
  ++_kevs;
  return true;
}

Hid_report::Key_event const *
Hid_report::get_key_event(unsigned idx) const
{
  if (idx < _kevs)
    return _kev + idx;

  return 0;
}

Hid_report::Key_event const *
Hid_report::find_key_event(int code) const
{
  for (unsigned i = 0; i < _kevs; ++i)
    if (_kev[i].code == code)
      return _kev + i;

  return 0;
}

void
Hid_report::remove_key_event(int code)
{
  unsigned i;
  for (i = 0; i < _kevs && _kev[i].code != code; ++i)
    ;

  if (i >= _kevs)
    return;

  memmove(_kev + i, _kev + i + 1, (_kevs - i - 1) * sizeof(Key_event));
  --_kevs;
}


void
User_state::set_pointer(int x, int y, bool hide)
{
  Point p(x, y);
  Rect scr(Point(0, 0), vstack()->canvas()->size());
  p = p.min(scr.p2());
  p = p.max(scr.p1());

  _mouse_pos = p;
  if (hide && _mouse_cursor->x1() < scr.x2())
    vstack()->viewport(_mouse_cursor, Rect(scr.p2(), _mouse_cursor->size()), true);

  if (!hide && _mouse_cursor->p1() != _mouse_pos)
    vstack()->viewport(_mouse_cursor, Rect(_mouse_pos, _mouse_cursor->size()), true);
}

#if 0
static void dump_stack(lua_State *l)
{
  int i = lua_gettop(l);
  while (i)
    {
      int t = lua_type(l, i);
      switch (t)
	{
	case LUA_TSTRING:
	  printf("#%02d: '%s'\n", i, lua_tostring(l, i));
	  break;
	case LUA_TBOOLEAN:
	  printf("#%02d: %s\n", i, lua_toboolean(l, i) ? "true" : "false");
	  break;
	case LUA_TNUMBER:
	  printf("#%02d: %g\n", i, lua_tonumber(l, i));
	  break;
	default:
	  printf("#%02d: [%s] %p\n", i, lua_typename(l, t), lua_topointer(l, i));
	  break;
	}

      --i;
    }
}
#endif

User_state::User_state(lua_State *lua, View_stack *_vstack, View *cursor)
: _vstack(_vstack), _mouse_pos(0,0), _keyboard_focus(0),
  _mouse_cursor(cursor), _l(lua)
{
  user_state = this;
  if (_mouse_cursor)
    {
      vstack()->push_top(_mouse_cursor, true);
      Point pos(vstack()->canvas()->size());
      vstack()->viewport(_mouse_cursor, Rect(pos, _mouse_cursor->size()), true);
    }
  vstack()->update_all_views();
  luaopen_Mag(lua);
}

User_state::~User_state()
{
  user_state = 0;
}

void
User_state::forget_view(View *v)
{
  vstack()->forget_view(v);
  for (View_proxy *p = _view_proxies; p; p = p->_n)
    p->forget(v);

  if (_keyboard_focus == v)
    _keyboard_focus = 0;

  if (_vstack->focused() == v)
    {
      _vstack->set_focused(0);
      _vstack->update_all_views();
    }
}

bool
User_state::set_focus(View *v)
{
  if (_keyboard_focus == v)
    return false;

  if (_keyboard_focus)
    _keyboard_focus->set_focus(false);
  _keyboard_focus = v;
  _vstack->set_focused(v);

  if (v)
    v->set_focus(true);

  return true;
}

int
User_state::get_input_stream_info_for_id(l4_umword_t id, Input_info *info) const
{
  lua_State *l = _l;
  int top = lua_gettop(l);
  lua_getglobal(l, "input_source_info");
  lua_pushinteger(l, id);
  if (lua_pcall(l, 1, 2, 0))
    {
      fprintf(stderr, "ERROR: lua event handling returned: %s.\n", lua_tostring(l, -1));
      lua_pop(l, 1);
      return -L4_ENOSYS;
    }

  int r = lua_tointeger(l, -2);
  if (r < 0)
    {
      lua_settop(l, top);
      return r;
    }


  L4Re::Event_stream_info *i = 0;
  swig_type_info *type = SWIG_TypeQuery(l, "L4Re::Event_stream_info *");
  if (!type)
    {
      fprintf(stderr, "Ooops: did not find type 'L4Re::Event_stream_info'\n");
      lua_settop(l, top);
      return -L4_EINVAL;
    }
    if (!SWIG_IsOK(SWIG_ConvertPtr(l, -1, (void**)&i, type, 0)))
    {
      fprintf(stderr, "Ooops: expected 'L4Re::Event_stream_info' as arg 2\n");
      lua_settop(l, top);
      return -L4_EINVAL;
    }

  if (!i)
    {
      lua_settop(l, top);
      return -L4_EINVAL;
    }

  *info = *i;
  lua_settop(l, top);
  return r;
}

int
User_state::get_input_axis_info(l4_umword_t id, unsigned naxes, unsigned const *axis,
                                Input_absinfo *info, unsigned char *ax_mode) const
{
  lua_State *l = _l;
  int top = lua_gettop(l);
  lua_getglobal(l, "input_source_abs_info");
  lua_pushinteger(l, id);
  for (unsigned i = 0; i < naxes; ++i)
    lua_pushinteger(l, axis[i]);

  if (lua_pcall(l, 1 + naxes, 1 + naxes, 0))
    {
      fprintf(stderr, "ERROR: lua event handling returned: %s.\n", lua_tostring(l, -1));
      lua_settop(l, top);
      return -L4_ENOSYS;
    }

  int r = lua_tointeger(l, top + 1);
  if (r < 0)
    {
      lua_settop(l, top);
      return r;
    }

  for (unsigned i = 0; i < naxes; ++i)
    {
      int ndx = top + i + 2;
      if (!lua_istable(l, ndx) && !lua_isuserdata(l, ndx))
	{
	  fprintf(stderr, "ERROR: lua event handling returned: %s.\n", lua_tostring(l, top + 1));
	  lua_settop(l, top);
	  return -L4_EINVAL;
	}

      lua_getfield(l, ndx, "value");
      info[i].value = lua_tointeger(l, -1);
      lua_pop(l, 1);

      lua_getfield(l, ndx, "min");
      info[i].min = lua_tointeger(l, -1);
      lua_pop(l, 1);

      lua_getfield(l, ndx, "max");
      info[i].max = lua_tointeger(l, -1);
      lua_pop(l, 1);

      lua_getfield(l, ndx, "fuzz");
      info[i].fuzz = lua_tointeger(l, -1);
      lua_pop(l, 1);

      lua_getfield(l, ndx, "flat");
      info[i].flat = lua_tointeger(l, -1);
      lua_pop(l, 1);

      lua_getfield(l, ndx, "resolution");
      info[i].resolution = lua_tointeger(l, -1);
      lua_pop(l, 1);

      if (ax_mode)
	{
	  lua_getfield(l, ndx, "mode");
	  ax_mode[i] = lua_tointeger(l, -1);
	  lua_pop(l, 1);
	}
    }

  lua_settop(l, top);
  return r;
}

}
