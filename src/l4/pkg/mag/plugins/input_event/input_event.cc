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

#include <l4/sys/capability>
#include <l4/re/console>
#include <l4/re/util/event>

namespace Mag_server {

class Input_source_event : public Input_source
{
public:
  L4::Cap<L4Re::Event> _ev_cap;
  L4Re::Util::Event _event;

  void core(Core_api *c) { _core = c; }

  void poll_events()
  {
    while (L4Re::Event_buffer::Event *e = _event.buffer().next())
      {
        post_event(e);
        e->free();
      }
  }

  int get_stream_info(l4_umword_t stream_id, L4Re::Event_stream_info *info)
  { return _ev_cap->get_stream_info_for_id(stream_id, info); }

  int get_axis_info(l4_umword_t stream_id, unsigned naxes, unsigned *axes,
                    L4Re::Event_absinfo *info)
  { return _ev_cap->get_axis_info(stream_id, naxes, axes, info); }
};

class Input_driver_event : public Input_driver
{
private:
  Input_source_event _source_be, _source_ev;

public:
  Input_driver_event() : Input_driver("L4Re::Event") {}

  void start(Core_api *core)
  {
    L4::Cap<L4Re::Event> c =
      L4::cap_dynamic_cast<L4Re::Event>(core->backend_fb());

    _source_be.core(core);
    _source_ev.core(core);

    if (c && !_source_be._event.init_poll(c))
      {
	_source_be._ev_cap = c;
        core->add_input_source(&_source_be);
      }

    c = L4Re::Env::env()->get_cap<L4Re::Event>("ev");
    if (c && !_source_ev._event.init_poll(c))
      {
	_source_ev._ev_cap = c;
	core->add_input_source(&_source_ev);
      }
  }
};

static Input_driver_event _evinput;

}
