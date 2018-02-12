/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "debug.h"
#include "vbus_factory.h"

namespace Vi {

Device *
Dev_factory::create(std::string const &_class)
{
  Name_map &m = name_map();
  Name_map::iterator i = m.find(_class);
  if (i == m.end())
    {
      d_printf(DBG_WARN, "WARNING: cannot create virtual device: '%s'\n",
               _class.c_str());
      return 0;
    }

  return i->second->vcreate();
}

}
