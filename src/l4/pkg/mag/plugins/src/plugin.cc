/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/mag/server/plugin>
#include <cstdio>

namespace Mag_server {

Plugin *Plugin::_first __attribute__((visibility("hidden")));

class Plugin_manager
{
public:
  static void init(Core_api *core)
  {
    printf("core-init\n");
    for (Plugin *p = Plugin::_first; p; p = p->_next)
      {
        printf("plugin: %p\n", p);
      if (!p->started())
	{
	  printf("starting %s plugin: %s\n", p->type(), p->name());
	  p->start(core);
	}
      }
  }
};


extern "C" void init_plugin(Core_api *core);

void init_plugin(Core_api *core)
{
  Plugin_manager::init(core);
}

}
