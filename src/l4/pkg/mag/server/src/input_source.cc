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
#include "plugin"
#include "lua_glue.swg.h"

#include <lua.h>
#include <lauxlib.h>
namespace Mag_server {

void
Input_source::post_event(L4Re::Event_buffer::Event const *e)
{
  lua_State *l = _core->lua_state();
  lua_getglobal(l, "handle_event");
  lua_createtable(l, 6, 0);
  lua_pushinteger(l, e->payload.type);
  lua_rawseti(l, -2, 1);
  lua_pushinteger(l, e->payload.code);
  lua_rawseti(l, -2, 2);
  lua_pushinteger(l, e->payload.value);
  lua_rawseti(l, -2, 3);
  lua_pushinteger(l, e->time);
  lua_rawseti(l, -2, 4);
  lua_pushinteger(l, e->payload.stream_id);
  lua_rawseti(l, -2, 5);
  lua_rawgeti(l, LUA_REGISTRYINDEX, _ref);
  lua_rawseti(l, -2, 6);

  if (lua_pcall(l, 1, 0, 0))
    {
      fprintf(stderr, "ERROR: lua event handling returned: %s.\n", lua_tostring(l, -1));
      lua_pop(l, 1);
    }
}


void
Input_source::add_lua_input_source(lua_State *l)
{
  swig_type_info *type = SWIG_TypeQuery(l, "Mag_server::Input_source *");
  assert(type);
  SWIG_NewPointerObj(l, this, type, 0);
  _ref = luaL_ref(l, LUA_REGISTRYINDEX);
}
}
