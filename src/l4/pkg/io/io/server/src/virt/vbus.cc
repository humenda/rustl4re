/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/env>
#include <l4/re/namespace>

#include <l4/re/error_helper>

#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vdevice-ops.h>
#include <l4/vbus/vbus_pm-ops.h>

#include <cstdio>

#include "debug.h"
#include "hw_msi.h"
#include "vbus.h"
#include "vmsi.h"
#include "vicu.h"
#include "server.h"
#include "res.h"
#include "cfg.h"
#include "vbus_factory.h"

Vi::System_bus::Root_resource_factory::Factory_list
  Vi::System_bus::Root_resource_factory::_factories(true);

namespace {

class Root_irq_rs : public Resource_space
{
private:
  Vi::System_bus *_bus;
  Vi::Sw_icu *_icu;

public:
  Root_irq_rs(Vi::System_bus *bus)
    : Resource_space(), _bus(bus), _icu(new Vi::Sw_icu())
  {
    _bus->add_child(_icu);
    _bus->sw_icu(_icu);
    _icu->name("L4ICU");
  }

  bool request(Resource *parent, Device *, Resource *child, Device *) override
  {
    if (0)
      {
        printf("VBUS: IRQ resource request: ");
        child->dump();
      }

    if (!parent)
      return false;

    if (!_icu)
      {
        _icu = new Vi::Sw_icu();
        _bus->add_child(_icu);
        _bus->sw_icu(_icu);
      }

    d_printf(DBG_DEBUG2, "Add IRQ resources to vbus: ");
    if (dlevel(DBG_DEBUG2))
      child->dump();

    _icu->add_irqs(child);
    return _bus->add_resource_to_bus(child);
  };

  bool alloc(Resource *parent, Device *,
             Resource *child, Device *, bool) override
  {
    d_printf(DBG_DEBUG2, "Allocate virtual IRQ resource ...\n");
    if (dlevel(DBG_DEBUG2))
      child->dump();

    Vi::Msi_resource *msi = dynamic_cast<Vi::Msi_resource*>(child);
    if (!msi || !parent)
      return false;

    d_printf(DBG_DEBUG2, "  Allocate Virtual MSI...\n");

    if (!_icu)
      {
        _icu = new Vi::Sw_icu();
        _bus->add_child(_icu);
      }

    int nr = _icu->alloc_irq(msi->flags(), msi->hw_msi());
    if (nr < 0)
      {
        d_printf(DBG_ERR, "ERROR: cannot allocate MSI resource\n");
        return false;
      }

    msi->start_end(nr, nr);
    msi->del_flags(Resource::F_disabled);

    if (dlevel(DBG_DEBUG2))
      {
        msi->dump(4);
        msi->hw_msi()->dump(4);
      }

    return _bus->add_resource_to_bus(msi);
  }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_irq_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_irq_rs\n");
    return false;
  }
};

class Root_x_rs : public Resource_space
{
private:
  Vi::System_bus *_bus;

public:
  Root_x_rs(Vi::System_bus *bus) : Resource_space(), _bus(bus)
  {}

  bool request(Resource *parent, Device *, Resource *child, Device *) override
  {
    if (0)
      {
        printf("VBUS: X resource request: ");
        child->dump();
      }

    if (!parent)
      return false;


    return _bus->add_resource_to_bus(child);
  }

  bool alloc(Resource *, Device *, Resource *, Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_x_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_x_rs\n");
    return false;
  }
};

class Root_dma_domain_rs : public Resource_space
{
private:
  Vi::System_bus *_bus;
  Dma_domain_group *_domain_group;

public:
  Root_dma_domain_rs(Vi::System_bus *bus, Dma_domain_group *g)
  : Resource_space(), _bus(bus), _domain_group(g)
  {}

  bool request(Resource *, Device *, Resource *child, Device *) override
  {
    Dma_domain *d = dynamic_cast<Dma_domain *>(child);
    d->add_to_group(_domain_group);
    return _bus->add_resource_to_bus(child);
  }

  bool alloc(Resource *, Device *, Resource *, Device *, bool) override
  { return false; }

  void assign(Resource *, Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_dma_domain_rs\n");
  }

  bool adjust_children(Resource *) override
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_dma_domain_rs\n");
    return false;
  }
};
}



namespace Vi {

System_bus::System_bus(Inhibitor_mux *mux)
: Inhibitor_provider(mux), _sw_icu(0)
{
  _handle = 0; // the vBUS root device always has handle 0
  _devices_by_id.push_back(this);
  register_property("num_msis", &_num_msis);
  add_feature(this);
  add_resource(new Root_resource(Resource::Irq_res, new Root_irq_rs(this)));
  Resource_space *x = new Root_x_rs(this);
  add_resource(new Root_resource(Resource::Mmio_res, x));
  add_resource(new Root_resource(Resource::Mmio_res | Resource::F_prefetchable, x));
  add_resource(new Root_resource(Resource::Io_res, x));
  add_resource(new Root_resource(Resource::Dma_domain_res,
                                 new Root_dma_domain_rs(this, &_dma_domain_group)));

  for (auto const *i: Root_resource_factory::_factories)
    add_resource(i->create(this));
}

System_bus::~System_bus() noexcept
{
  registry->unregister_obj(this);
  // FIXME: must delete all devices
}

/**
 * \brief Add the region described by \a r to the VBUS resources.
 *
 * Adds the resource to the resources available through this V-BUS.
 * This also supports overlapping resources by merging them together.
 */
bool
System_bus::add_resource_to_bus(Resource *r)
{
  if (_resources.count(r) == 0)
    {
      _resources.insert(r);
      return true;
    }

  // at least one overlapping resource entry found
  auto lower = _resources.lower_bound(r);
  if (typeid (*r) != typeid (**lower))
    {
      if (dlevel(DBG_ERR))
        {
          printf("error: overlapping incompatible resources for vbus\n");
          printf("       new:   "); r->dump(); puts(" conflicts with");
          printf("       found: "); (*lower)->dump(); puts("");
        }
      return false;
    }

  if ((*lower)->contains(*r))
    // already fully included
    return true;

  auto upper = _resources.upper_bound(r);
  for (auto it = lower; it != upper; ++it)
    {
      bool same_type = typeid (*r) == typeid (**it);
      if (!same_type || !r->contains(**it))
        {
          if (dlevel(DBG_ERR))
            {
              printf("error: %s resources for vbus\n",
                     same_type ? "oddly overlapping" : "overlapping incompatible");
              printf("       new:   "); r->dump(); puts(" conflicts with");
              printf("       found: "); (*it)->dump(); puts("");
            }
          return false;
        }
    }

  // r is a superset of [lower, upper); replace existing elements with r
  _resources.erase(lower, upper);
  _resources.insert(r);
  return true;
}

bool
System_bus::resource_allocated(Resource const *r) const
{
  if (r->disabled())
    return false;

  auto i = _resources.find(const_cast<Resource *>(r));
  if (i == _resources.end())
    return false;

  return (*i)->contains(*r);
}

int
System_bus::pm_suspend()
{
  inhibitor_release(L4VBUS_INHIBITOR_SUSPEND);

  return 0;
}

int
System_bus::pm_resume()
{
  inhibitor_acquire(L4VBUS_INHIBITOR_SUSPEND, "vbus active");

  return 0;
}

void
System_bus::dump_resources() const
{
  for (auto i = _resources.begin();
       i != _resources.end(); ++i)
    (*i)->dump();
}

int
System_bus::request_resource(L4::Ipc::Iostream &ios)
{
  l4vbus_resource_t res;
  if (!ios.get(res))
    return -L4_EMSGTOOSHORT;

  Resource ires(res.type, res.start, res.end);
  if (!ires.valid())
    return -L4_EINVAL;

  if (dlevel(DBG_DEBUG2))
    {
      printf("request resource: ");
      ires.dump();
      puts("");
    }

  auto i = _resources.find(&ires);

  if (0)
    dump_resources();

  if (i == _resources.end() || !(*i)->contains(ires))
    return -L4_ENOENT;

  if (0)
    if (dlevel(DBG_DEBUG))
      {
        printf("  found resource: ");
        (*i)->dump();
        puts("");
      }

  if (res.type == L4VBUS_RESOURCE_PORT)
    {
      l4_uint64_t sz = res.end + 1 - res.start;

      int szl2 = 0;
      while ((1UL << szl2) < sz)
        ++szl2;

      if ((1UL << szl2) > sz)
        --szl2;

      ios << L4::Ipc::Snd_fpage::io(res.start, szl2, L4_FPAGE_RWX);
      return L4_EOK;
    }


  return -L4_ENOENT;
}

int
System_bus::assign_dma_domain(L4::Ipc::Iostream &ios)
{
  l4_msgtag_t tag = ios.tag();
  if (!tag.items())
    return -L4_EINVAL;

  if (tag.words() > L4::Ipc::Msg::Mr_words - L4::Ipc::Msg::Item_words)
    return -L4_EMSGTOOLONG;

  unsigned id, flags;
  ios >> id >> flags;

  L4::Ipc::Snd_fpage spc;
  int r = L4::Ipc::Msg::msg_get(
      (char *)&l4_utcb_mr_u(ios.utcb())->mr[tag.words()],
      0, L4::Ipc::Msg::Item_bytes, spc);

  if (r < 0)
    return r;

  if (!spc.cap_received())
    return -L4_EINVAL;

  L4::Cap<void> spc_cap = server_iface()->get_rcv_cap(0);

  Dma_domain_if *d = 0;

  if (id == ~0U)
    {
      d = _dma_domain_group.get();
      if (!d)
        {
          d_printf(DBG_WARN, "vbus %s does not support a global DMA domain\n",
                   name());
          return -L4_ENOENT;
        }
    }
  else
    {
      Resource ires(Resource::Dma_domain_res, id, id);
      auto i = _resources.find(&ires);
      if (i == _resources.end() || !(*i)->contains(ires))
        return -L4_ENOENT;

      d = dynamic_cast<Dma_domain_if *>(*i);
      if (!d)
        {
          d_printf(DBG_ERR, "%s:%d: error: internal IO error,"
                            " DMA resource not of a Dma_domain\n",
                   __FILE__, __LINE__);
          return -L4_EINVAL;
        }
    }

  int res;
  bool is_bind = flags & L4VBUS_DMAD_BIND;
  if (flags & L4VBUS_DMAD_KERNEL_DMA_SPACE)
    res = d->set_dma_task(is_bind, L4::cap_cast<L4::Task>(spc_cap));
  else
    res = d->set_dma_space(is_bind, L4::cap_cast<L4Re::Dma_space>(spc_cap));

  if (res >= 0 && is_bind)
    server_iface()->realloc_rcv_cap(0);

  return res;
}

int
System_bus::op_map(L4Re::Dataspace::Rights,
                   L4Re::Dataspace::Offset offset,
                   L4Re::Dataspace::Map_addr spot,
                   L4Re::Dataspace::Flags flags,
                   L4::Ipc::Snd_fpage &fp)
{

  Resource pivot(L4VBUS_RESOURCE_MEM, offset, offset);
  auto r = _resources.find(&pivot);

  if (r == _resources.end())
    {
      if (dlevel(DBG_INFO))
        {
          printf("request: no MMIO resource at %llx\n", offset);
          printf("Available resources:\n");
          dump_resources();
        }
      return -L4_ERANGE;
    }

  offset = l4_trunc_page(offset);

  l4_addr_t st = l4_trunc_page((*r)->start());
  l4_addr_t adr = (*r)->map_iomem();

  if (!adr)
    return -L4_ENOMEM;

  adr = l4_trunc_page(adr);

  l4_addr_t addr = offset - st + adr;
  unsigned char order
    = l4_fpage_max_order(L4_PAGESHIFT,
        addr, addr, addr + (*r)->size(), spot);

  L4::Ipc::Snd_fpage::Cacheopt f;

  // if cached_mem():  Cached
  // else:             client gets what it requests, the default is Uncached
  using L4Re::Dataspace;
  if ((*r)->cached_mem())
    {
      if ((flags & Dataspace::F::Caching_mask) != Dataspace::F::Cacheable)
        {
          d_printf(DBG_ERR,
                   "MMIO resource at 0x%llx requested as 'uncached' or 'bufferable' "
                   "but marked as cachable only.\n",
                   offset);
          return -L4_EINVAL;
        }
      f = L4::Ipc::Snd_fpage::Cached;
    }
  else
    {
      switch ((flags & Dataspace::F::Caching_mask).raw)
        {
        case Dataspace::F::Bufferable:
          f = L4::Ipc::Snd_fpage::Buffered;
          break;
        case Dataspace::F::Cacheable:
          f = L4::Ipc::Snd_fpage::Cached;
          break;
        default:
          f = L4::Ipc::Snd_fpage::Uncached;
          break;
        }
    }

  unsigned char rights = (*r)->flags() & Resource::Mem_type_read_only
                         ? L4_FPAGE_RO : L4_FPAGE_RWX;

  fp = L4::Ipc::Snd_fpage::mem(l4_trunc_size(addr, order), order,
                               rights, l4_trunc_page(spot),
                               L4::Ipc::Snd_fpage::Map,
                               f);
  return L4_EOK;
};


long
System_bus::op_acquire(L4Re::Inhibitor::Rights, l4_umword_t id, L4::Ipc::String<> reason)
{
  inhibitor_acquire(id, reason.data);
  return L4_EOK;
}

long
System_bus::op_release(L4Re::Inhibitor::Rights, l4_umword_t id)
{
  inhibitor_release(id);
  return L4_EOK;
}

long
System_bus::op_next_lock_info(L4Re::Inhibitor::Rights, l4_mword_t &id,
                              L4::Ipc::String<char> &name)
{
  ++id;
  if (id >= L4VBUS_INHIBITOR_MAX)
    return -L4_ENODEV;

  static char const *const names[] =
    {
      /* [L4VBUS_INHIBITOR_SUSPEND]  = */ "suspend",
      /* [L4VBUS_INHIBITOR_SHUTDOWN] = */ "shutdown",
      /* [L4VBUS_INHIBITOR_WAKEUP]   = */ "wakeup"
    };

  name.copy_in(names[id]);
  return L4_EOK;

}


// some internal helper functions for device RPCs
namespace {

template<typename V, typename S> inline V
rpc_get(S &ios)
{
  V v;
  if (ios.get(v))
    return v;

  L4Re::throw_error(-L4_EMSGTOOSHORT);
}

// get the HID of a device
static inline int
rpc_get_dev_hid(Device *dev, L4::Ipc::Iostream &ios)
{
  char const *h = dev->hid();
  if (!h)
    return -L4_ENOSYS;

  ios << h;
  return L4_EOK;
}

// get the ADR record of a device
static inline int
rpc_get_dev_adr(Device *dev, L4::Ipc::Iostream &ios)
{
  l4_uint32_t a = dev->adr();
  if (a == l4_uint32_t(~0))
    return -L4_ENOSYS;

  ios << a;
  return L4_EOK;
}

}

Device *
System_bus::dev_from_id(l4vbus_device_handle_t dev, int err) const
{
  if (dev >= 0 && dev < (int)_devices_by_id.size())
    if (Device *d = _devices_by_id[dev])
      return d;

  L4Re::throw_error(err);
}

Device::iterator
System_bus::rpc_get_dev_next_iterator(Device *dev, L4::Ipc::Iostream &ios, int err) const
{
  auto current = rpc_get<l4vbus_device_handle_t>(ios);
  int depth  = rpc_get<int>(ios);
  iterator c;

  if (current == L4VBUS_NULL)
    c = dev->begin(depth);
  else
    {
      c = iterator(dev, dev_from_id(current, -L4_EINVAL), depth);

      if (c == dev->end())
        L4Re::throw_error(err);

      ++c;
    }

  if (c == dev->end())
    L4Re::throw_error(err);

  return c;
}

inline int
System_bus::rpc_device_get(Device *dev, L4::Ipc::Iostream &ios) const
{
  ios.put(dev->handle());
  ios.put(dev->get_device_info());
  return L4_EOK;
}

inline int
System_bus::rpc_get_next_dev(Device *dev, L4::Ipc::Iostream &ios, int err) const
{
  return rpc_device_get(*rpc_get_dev_next_iterator(dev, ios, err), ios);
}

int
System_bus::rpc_get_dev_by_hid(Device *dev, L4::Ipc::Iostream &ios) const
{
  auto c = rpc_get_dev_next_iterator(dev, ios, -L4_ENOENT);

  unsigned long sz;
  char const *hid = 0;

  ios >> L4::Ipc::buf_in(hid, sz);

  if (0)
    printf("look for '%s' in %p\n", hid, this);

  if (!hid || sz <= 1)
    return -L4_EINVAL;

  for (; c != end(); ++c)
    if (char const *h = c->hid())
      if (strcmp(h, hid) == 0)
        return rpc_device_get(*c, ios);

  return -L4_ENOENT;
}

int
System_bus::dispatch_generic(L4vbus::Vbus::Rights obj, Device *dev,
                             l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  switch (l4vbus_subinterface(func))
    {
    case L4VBUS_INTERFACE_GENERIC:
      switch ((L4vbus_vdevice_op)func)
        {
        case L4vbus_vdevice_hid:
          return rpc_get_dev_hid(dev, ios);
        case L4vbus_vdevice_adr:
          return rpc_get_dev_adr(dev, ios);
        case L4vbus_vdevice_get_by_hid:
          return rpc_get_dev_by_hid(dev, ios);
        case L4vbus_vdevice_get_next:
          return rpc_get_next_dev(dev, ios, -L4_ENODEV);
        case L4vbus_vdevice_get:
          return rpc_device_get(dev, ios);

        case L4vbus_vdevice_get_resource:
          ios.put(dev->get_resource_info(rpc_get<int>(ios)));
          return L4_EOK;

        case L4vbus_vdevice_is_compatible:
            {
              unsigned long sz;
              char const *cid = 0;
              ios >> L4::Ipc::buf_in(cid, sz);
              if (sz == 0)
                return -L4_EMSGTOOSHORT;
              return dev->match_cid(cxx::String(cid, strnlen(cid, sz))) ? 1 : 0;
            }

        case L4vbus_vdevice_get_hid:
          if (char const *_hid = dev->hid())
            ios << _hid;
          else
            ios << "";

          return L4_EOK;

        default: return -L4_ENOSYS;
        }

    case L4VBUS_INTERFACE_PM:
      switch (func)
        {
        case L4VBUS_PM_OP_SUSPEND:
          return dev->pm_suspend();

        case L4VBUS_PM_OP_RESUME:
          return dev->pm_resume();

        default: return -L4_ENOSYS;
        }

    default:
      break;
    }

  for (auto *i: *dev->features())
    {
      int e = i->dispatch(obj & L4_CAP_FPAGE_RS, func, ios);
      if (e != -L4_ENOSYS)
        return e;
    }

  return -L4_ENOSYS;
}

l4_msgtag_t
System_bus::op_dispatch(l4_utcb_t *utcb, l4_msgtag_t tag, L4vbus::Vbus::Rights obj)
{
  L4::Ipc::Iostream ios(utcb);
  ios.Istream::tag() = tag;

  Device *dev = dev_from_id(rpc_get<l4vbus_device_handle_t>(ios), -L4_ENODEV);
  l4_uint32_t func = rpc_get<l4_uint32_t>(ios);
  return ios.prepare_ipc(dispatch_generic(obj, dev, func, ios));
}

int
System_bus::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  switch (func)
    {
    case L4vbus_vbus_request_resource:
      return request_resource(ios);
    case L4vbus_vbus_assign_dma_domain:
      return assign_dma_domain(ios);
    default:
      return -L4_ENOSYS;
    }
}

Vbus_event_source::Vbus_event_source()
{
  using L4Re::Util::Unique_cap;
  using L4Re::chkcap;
  using L4Re::chksys;

  auto buffer_ds = chkcap(L4Re::Util::make_unique_cap<L4Re::Dataspace>(),
             "allocate event-buffer data-space capability");

  chksys(L4Re::Env::env()->mem_alloc()->alloc(L4_PAGESIZE, buffer_ds.get()),
         "allocate event-buffer data-space");

  chksys(buffer.attach(buffer_ds.get(), L4Re::Env::env()->rm()),
         "attach event-buffer data-space");

  _ds = buffer_ds.release();
}

Io_irq_pin::Msi_src *
System_bus::find_msi_src(Msi_src_info si)
{
  if (si.is_dev_handle())
    {
      Device *dev = dev_from_id(si.dev_handle(), -L4_EINVAL);
      if (Msi_src_feature *msi = dev->find_feature<Msi_src_feature>())
        return msi->msi_src();
    }
  else if (si.svt())
    return Device::find_msi_src(si);

  d_printf(DBG_ALL, "%s: device has no MSI support\n", __func__);
  return nullptr;
}

/**
 * \brief Send device notification to client.
 *
 * \param dev device the notification is originating from.
 * \param type event type to be sent, see event_enums.h for types.
 * \param event event ID.
 * \param value value for the event.
 * \param syn trigger an IRQ for the event if true.
 */
bool
System_bus::dev_notify(Device const *dev, unsigned type,
                       unsigned event, unsigned value, bool syn)
{
  Vbus_event_source::Event ev;
  ev.time = l4_kip_clock(l4re_kip());
  ev.payload.type = type;
  ev.payload.code = event;
  ev.payload.value = value;
  ev.payload.stream_id = dev->handle();

  return Vbus_event_source::put(ev, syn);
}

int
System_bus::get_stream_info_for_id(l4_umword_t dev_id, L4Re::Event_stream_info *info)
{
  Device *dev = dev_from_id(dev_id, -L4_ENOSYS);
  Io::Event_source_infos const *_info = dev->get_event_infos();
  if (!_info)
    return -L4_ENOSYS;

  *info = _info->info;
  return 0;
}

int
System_bus::get_stream_state_for_id(l4_umword_t dev_id, L4Re::Event_stream_state *state)
{
  Device *dev = dev_from_id(dev_id, -L4_ENOSYS);
  Io::Event_source_infos const *_info = dev->get_event_infos();
  if (!_info)
    return -L4_ENOSYS;

  *state = _info->state;
  return 0;
}

void
System_bus::inhibitor_signal(l4_umword_t id)
{
  // FIXME: need a subscribed flag!
  // only signal if this inhibitor is actually acquired
  if (!inhibitor_acquired(id))
    return;

  Vbus_event_source::Event ev;
  ev.time = l4_kip_clock(l4re_kip());
  ev.payload.type = L4RE_EV_PM;
  ev.payload.code = id;
  ev.payload.value = 1;
  ev.payload.stream_id = _handle;

  put(ev);
}

void
System_bus::finalize()
{
  for (auto d = begin(L4VBUS_MAX_DEPTH); d != end(); ++d)
    if (d->handle() < 0)
      {
        d->set_handle(_devices_by_id.size());
        _devices_by_id.push_back(*d);
      }
}

}
