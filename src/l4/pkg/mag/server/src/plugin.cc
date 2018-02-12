/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "plugin"

#include <lua.h>
#include <lauxlib.h>

#include <l4/cxx/string>
#include <l4/mag/server/session>
#include <l4/re/error_helper>

#include <algorithm>
#include <cstring>


namespace Mag_server {

Plugin *Plugin::_first;

Core_api::Core_api(lua_State *lua, User_state *u,
                   L4::Cap<L4Re::Video::Goos> fb,
		   Mag_gfx::Font const *label_font)
: _ust(u), _fb(fb), _lua(lua), _label_font(label_font)
{
}

namespace {
  Session::Property_handler const _default_session_props[] =
    { { "l",     true, &Session::set_label_prop },
      { "label", true, &Session::set_label_prop },
      { "col",   true, &Session::set_color_prop },
      { 0, 0, 0 }
    };

  static bool handle_option(Session *s, Session::Property_handler const *p, cxx::String const &a)
  {
    for (; p && p->tag; ++p)
      {
	cxx::String::Index v = a.starts_with(p->tag);
	if (v && (!p->value_property || a[v] == '='))
	  {
	    p->handler(s, p, a.substr(v + 1));
	    return true;
	  }
      }
    return false;
  }
}

void
Core_api::set_session_options(Session *s, L4::Ipc::Varg_list_ref args,
                              Session::Property_handler const *extra) const
{
  for (;;)
    {
      L4::Ipc::Varg opt = args.next();
      if (!opt.tag())
        break;

      if (!opt.is_of<char const *>())
	{
	  printf("skipping non string argument for session!\n");
	  continue;
	}

      // opt without zero terminator
      cxx::String a(opt.value<char const *>(), opt.length() - 1);

      if (!handle_option(s, _default_session_props, a)
	  && !handle_option(s, extra, a))
	{
	  printf("unknown session option '%.*s'\n", a.len(), a.start());
	  L4Re::chksys(-L4_EINVAL, "parsing session options");
	}
    }

  if (!s->label())
    s->set_label_prop(s, 0, "<empty>");
}

}

