/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "client.h"
#include "server.h"

#include <l4/re/util/object_registry>
#include <l4/l4virtio/server/l4virtio>
#include <l4/l4virtio/l4virtio>
#include <l4/sys/cxx/ipc_epiface>

class Virtio_cons
: public L4virtio::Svr::Device,
  public Client,
  public L4::Epiface_t<Virtio_cons, L4virtio::Device, Server_object>
{
private:
  enum { Rx, Tx };

  struct Buffer : L4virtio::Svr::Data_buffer
  {
    Buffer() = default;
    Buffer(L4virtio::Svr::Driver_mem_region const *r,
           L4virtio::Svr::Virtqueue::Desc const &d,
           L4virtio::Svr::Request_processor const *)
    {
      pos = static_cast<char *>(r->local(d.addr));
      left = d.len;
    }
  };

  struct Host_irq : public L4::Irqep_t<Host_irq>
  {
    explicit Host_irq(Virtio_cons *s) : s(s) {}
    Virtio_cons *s;
    void handle_irq()
    { s->kick(); }
  };

  Host_irq _host_irq;
  L4virtio::Svr::Dev_config _dev_config;

  L4virtio::Svr::Virtqueue _q[2];
  L4Re::Util::Unique_cap<L4::Irq> kick_guest_irq;

  enum { Default_obuf_size = 40960 };
  static unsigned _dfl_obufsz;

  enum { Max_desc = 0x100 };

public:
  Virtio_cons(std::string const &name, int color, size_t bufsz, Key key,
              L4Re::Util::Object_registry *r)
  : L4virtio::Svr::Device(&_dev_config),
    Client(name, color, 512, bufsz < 512 ? _dfl_obufsz : bufsz, key),
    _host_irq(this),
    _dev_config(0x44, L4VIRTIO_ID_CONSOLE, 0x20, 2)
  {
    reset_queue_config(0, Max_desc);
    reset_queue_config(1, Max_desc);
    r->register_irq_obj(&_host_irq);
    init_mem_info(40);
    _attr.l_flags = 0;
    _attr.i_flags = 0;
    _attr.o_flags = 0;
  }

  void register_single_driver_irq()
  {
    kick_guest_irq = L4Re::Util::Unique_cap<L4::Irq>(
        L4Re::chkcap(server_iface()->rcv_cap<L4::Irq>(0)));
    L4Re::chksys(server_iface()->realloc_rcv_cap(0));
  }

  void trigger_driver_config_irq() const override
  {
    kick_guest_irq->trigger();
  }

  L4::Cap<L4::Irq> device_notify_irq() const
  { return L4::cap_cast<L4::Irq>(_host_irq.obj_cap()); }

  void reset()
  {
    for (L4virtio::Svr::Virtqueue &q: _q)
      q.disable();
  }

  template<typename T, unsigned N >
  static unsigned array_length(T (&)[N]) { return N; }

  int reconfig_queue(unsigned index)
  {
    if (index >= array_length(_q))
      return -L4_ERANGE;

    if (setup_queue(_q + index, index, Max_desc))
      return 0;

    return -L4_EINVAL;
  }

  bool check_queues()
  {
    for (L4virtio::Svr::Virtqueue &q: _q)
      if (!q.ready())
        {
          reset();
          printf("failed to start queues\n");
          return false;
        }

    return true;
  }

  void notify_queue(L4virtio::Svr::Virtqueue *queue)
  {
    //printf("%s\n", __func__);
    if (queue->no_notify_guest())
      return;

    // we do not care about this anywhere, so skip
    // _device_config->irq_status |= 1;

    kick_guest_irq->trigger();
  }

  bool collected() { return Client::collected(); }

  void kick();
  void handle_tx();
  void handle_rx();
  void trigger() const { const_cast<Virtio_cons*>(this)->kick(); }

  static void default_obuf_size(unsigned bufsz)
  {
    _dfl_obufsz = cxx::max(512U, cxx::min(16U << 20, bufsz));
  }

  Server_iface *server_iface() const
  { return Server_object::server_iface(); }
};

