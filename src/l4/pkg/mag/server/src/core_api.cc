// vi:ft=cpp
/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "core_api"
#include <lua.h>

namespace Mag_server {

void
Core_api_impl::add_input_source(Input_source *i)
{
  i->_next_active = _input;
  _input = i;
  i->add_lua_input_source(lua_state());
}

void
Core_api_impl::register_session(Session *s) const
{
  _sessions.push_back(s);
  s->set_notifier(&_session_ntfy);
  _session_ntfy.notify();
}

void
Core_api_impl::get_ticks(cxx::Observer *o) const
{
  _tick_ntfy.add(o);
}

void
Core_api_impl::add_session_observer(cxx::Observer *o) const
{
  _session_ntfy.add(o);
}

}
