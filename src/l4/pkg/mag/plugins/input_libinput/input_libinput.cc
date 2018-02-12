/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/mag/server/input_driver>
#include <l4/mag/server/input_source>

#include <l4/input/libinput.h>

namespace {
using Mag_server::Input_driver;
using Mag_server::Input_source;
using Mag_server::Core_api;
using Mag_server::User_state;

class Input_driver_libinput : public Input_driver, public Input_source
{
public:
  Input_driver_libinput() : Input_driver("libinput") {}
  void start(Core_api *core)
  {
    if (l4input_init(0xff, 0) == 0)
      {
	_core = core;
	core->add_input_source(this);
      }
  }

  void poll_events()
  {
    enum { N=20 };
    L4Re::Event_buffer::Event _evb[N];
    while (l4input_ispending())
      {
	L4Re::Event_buffer::Event *e = _evb;
	for (int max = l4input_flush(_evb, N); max; --max, ++e)
	  post_event(e);
      }
  }

  int get_stream_info(l4_umword_t stream_id, L4Re::Event_stream_info *info)
  { return l4evdev_stream_info_for_id(stream_id, info); }
  int get_axis_info(l4_umword_t stream_id, unsigned naxes, unsigned *axes,
                    L4Re::Event_absinfo *info)
  { return l4evdev_absinfo(stream_id, naxes, axes, info); }
};

static Input_driver_libinput _libinput;
}
