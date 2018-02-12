/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <set>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/hlist>

#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_interfaces.h>
#include <l4/vbus/vbus>
#include <l4/re/util/event_svr>
#include <l4/re/util/event_buffer>

#include <pthread.h>

#include "vdevice.h"
#include "device.h"
#include "inhibitor_mux.h"
#include "dma_domain.h"
#include "../utils.h"

namespace Vi {
class Sw_icu;

struct Vbus_event_source : L4Re::Util::Event_svr<Vbus_event_source>
{
  typedef L4Re::Util::Event_buffer::Event Event;
  L4Re::Util::Event_buffer buffer;
  pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  void reset_event_buffer()
  {
    Pthread_mutex_guard g(&lock);
    buffer.reset();
  }

  virtual L4::Epiface::Server_iface *server_iface() const = 0;

  Vbus_event_source();

  bool put(Event const &ev, bool trigger = true)
  {
    bool res;
      {
        Pthread_mutex_guard g(&lock);
        res = buffer.put(ev);
      }

    if (res && trigger)
      _irq.trigger();
    return res;
  }

  virtual int get_stream_info_for_id(l4_umword_t, L4Re::Event_stream_info *) = 0;
  virtual int get_stream_state_for_id(l4_umword_t, L4Re::Event_stream_state *) = 0;
};

class System_bus :
  public Device,
  public Dev_feature,
  public L4::Epiface_t<System_bus, L4vbus::Vbus>,
  public Inhibitor_provider,
  public Vbus_event_source
{
public:
  // Factory to create root resource objects
  struct Root_resource_factory : cxx::H_list_item_t<Root_resource_factory>
  {
    virtual Root_resource *create(System_bus *bus) const = 0;
    Root_resource_factory()
    { _factories.push_front(this); }

    typedef cxx::H_list_t<Root_resource_factory> Factory_list;
    static Factory_list _factories;
  };

  // template to create a factory for root objects of type
  // \a TYPE.
  template< unsigned TYPE, typename RS >
  struct Root_resource_factory_t : Root_resource_factory
  {
    Root_resource *create(System_bus *bus) const
    {
      return new Root_resource(TYPE, new RS(bus));
    }
  };

  // comparison operation to sort resources
  struct Res_cmp
  {
    bool operator () (Resource const *a, Resource const *b) const
    {
      if (a->type() == b->type())
        return a->lt_compare(b);
      return a->type() < b->type();
    }
  };

  // Set of all resources of all types
  typedef std::set<Resource*, Res_cmp> Resource_set;


  // Vi::System_bus interface
  explicit System_bus(Inhibitor_mux *hw_bus);
  ~System_bus() noexcept;

  void dump_resources() const;
  bool add_resource_to_bus(Resource *r);
  Resource_set const *resource_set() const { return &_resources; }
  Sw_icu *sw_icu() const { return _sw_icu; }
  void sw_icu(Sw_icu *icu) { _sw_icu = icu; }

  char const *inhibitor_name() const override
  { return Device::name(); }

  // L4::Server_object interface
  int op_clear(L4Re::Dataspace::Rights, l4_addr_t, unsigned long)
  { return -L4_ENOSYS; }

  using Vbus_event_source::op_info;
  int op_info(L4Re::Dataspace::Rights, L4Re::Dataspace::Stats &)
  { return -L4_ENOSYS; }

  int op_take(L4Re::Dataspace::Rights)
  { return 0; }

  int op_release(L4Re::Dataspace::Rights)
  { return 0; }

  int op_allocate(L4Re::Dataspace::Rights, l4_addr_t, l4_size_t)
  { return 0; }

  int op_copy_in(L4Re::Dataspace::Rights, l4_addr_t, L4::Ipc::Snd_fpage, l4_addr_t,
                 unsigned long)
  { return -L4_ENOSYS; }

  int op_phys(L4Re::Dataspace::Rights, l4_addr_t,
              l4_addr_t &, l4_size_t &)
  { return -L4_ENOSYS; }

  int op_map(L4Re::Dataspace::Rights r, long unsigned offset,
             l4_addr_t spot, unsigned long flags,
             L4::Ipc::Snd_fpage &fp);


  long op_acquire(L4Re::Inhibitor::Rights r, l4_umword_t id,
                  L4::Ipc::String<> reason);

  long op_release(L4Re::Inhibitor::Rights r, l4_umword_t id);

  long op_next_lock_info(L4Re::Inhibitor::Rights r, l4_mword_t &id,
                         L4::Ipc::String<char> &name);

  l4_msgtag_t op_dispatch(l4_utcb_t *utcb, l4_msgtag_t tag, L4vbus::Vbus::Rights r);

  Server_iface *server_iface() const override
  { return L4::Epiface::server_iface(); }

  // Vi::Device interface
  l4_uint32_t interface_type() const override
  { return 1 << L4VBUS_INTERFACE_BUS; }

  using L4::Epiface::dispatch;
  int dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios) override;
  bool match_hw_feature(Hw::Dev_feature const *) const override
  { return false; }


  // Dev_feature interface
  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }


  int pm_suspend() override;
  int pm_resume() override;


  // Device interface
  bool resource_allocated(Resource const *) const override;


  // Pm_client interface
  void pm_notify(unsigned, unsigned) {}

  bool dev_notify(Device const *dev, unsigned type,
                  unsigned event, unsigned value, bool syn) override;

  void inhibitor_signal(l4_umword_t id) override;

private:
  int request_resource(L4::Ipc::Iostream &ios);
  int request_iomem(L4::Ipc::Iostream &ios);
  int assign_dma_domain(L4::Ipc::Iostream &ios);

  int get_stream_info_for_id(l4_umword_t, L4Re::Event_stream_info *) override;
  int get_stream_state_for_id(l4_umword_t, L4Re::Event_stream_state *) override;

  Resource_set _resources;
  Device *_host;
  Sw_icu *_sw_icu;
  Int_property _num_msis;
  Dma_domain_group _dma_domain_group;
};

}
