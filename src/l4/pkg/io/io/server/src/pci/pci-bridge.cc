/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */
#include <pci-bridge.h>
#include <pci-caps.h>
#include <pci-driver.h>
#include <resource_provider.h>

namespace Hw { namespace Pci {

namespace {

static inline l4_uint32_t
devfn(unsigned dev, unsigned fn)
{ return (dev << 16) | fn; }

}

void
Irq_router::dump(int indent) const
{
  printf("%*sPCI IRQ ROUTER: %s (%p)\n", indent, "", typeid(*this).name(),
         this);
}

bool
Pci_pci_bridge_irq_router_rs::request(Resource *parent, ::Device *pdev,
                                      Resource *child, ::Device *cdev)
{
  bool res = false;

  Hw::Device *cd = dynamic_cast<Hw::Device*>(cdev);

  if (!cd)
    return res;

  if (pdev->parent())
    {
      child->start((child->start() + (cd->adr() >> 16)) & 3);
      res = pdev->parent()->request_child_resource(child, pdev);
      if (res)
        child->parent(parent);
    }

  return res;
}

void
Generic_bridge::check_bus_config()
{
  auto c = config();

  // the config space address for bus config is the same in
  // type 1 and type 2
  l4_uint32_t b = c.read<l4_uint32_t>(Config::Primary);

  l4_uint8_t pb = b & 0xff;
  l4_uint8_t sb = (b >> 8) & 0xff;
  l4_uint8_t so = (b >> 16) & 0xff;

  // set internal attributes
  pri = pb;
  num = sb;
  subordinate = so;

  if (pb == cfg.addr().bus() && sb > cfg.addr().bus()
      && _bridge->check_bus_number(sb))
    return; // nothing to do primary and secondary bus numbers are sane.

  unsigned new_so = _bridge->alloc_bus_number();
  if (new_so == 0)
    assert (false);

  pri = cfg.addr().bus();
  num = new_so;
  subordinate = new_so;
  b &= 0xff000000;
  b |= static_cast<l4_uint32_t>(pri);
  b |= static_cast<l4_uint32_t>(num) << 8;
  b |= static_cast<l4_uint32_t>(subordinate) << 16;

  c.write(Config::Primary, b);
}

void
Bridge::setup_children(Hw::Device *)
{
  auto c = config();
  if (!mmio->empty() && mmio->valid())
    {
      l4_uint32_t v = (mmio->start() >> 16) & 0xfff0;
      v |= mmio->end() & 0xfff00000;
      c.write<l4_uint32_t>(Config::Mem_base, v);
      if (0)
        printf("%08x: set mmio to %08x\n", host()->adr(), v);
      if (0)
        printf("%08x: mmio =      %08x\n", host()->adr(), c.read<l4_uint32_t>(0x20));

      c.write<l4_uint16_t>(Config::Command, c.read<l4_uint16_t>(0x04) | 3);
    }

  if (!pref_mmio->empty() && pref_mmio->valid())
    {
      l4_uint32_t v = (pref_mmio->start() >> 16) & 0xfff0;
      v |= pref_mmio->end() & 0xfff00000;
      c.write<l4_uint32_t>(Config::Pref_mem_base, v);
      if (0)
        printf("%08x: set pref mmio to %08x\n", host()->adr(), v);
    }

  enable_bus_master();
}

void
Bridge::discover_resources(Hw::Device *host)
{
  if (flags.discovered())
    return;

  auto c = config();

  l4_uint32_t v;
  l4_uint64_t s, e;

  v = c.read<l4_uint32_t>(Config::Mem_base);
  s = (v & 0xfff0) << 16;
  e = (v & 0xfff00000) | 0xfffff;

  Resource *r = new Resource_provider(Resource::Mmio_res | Resource::F_can_move
                                      | Resource::F_can_resize);
  r->set_id("WIN0");
  r->alignment(0xfffff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();

  if (0)
    printf("%08x: mmio = %08x\n", _host->adr(), v);
  mmio = r;
  mmio->validate();
  _host->add_resource_rq(mmio);

  r = new Resource_provider(Resource::Mmio_res | Resource::F_prefetchable
                            | Resource::F_can_move | Resource::F_can_resize);
  r->set_id("WIN1");
  v = c.read<l4_uint32_t>(Config::Pref_mem_base);
  s = (v & 0xfff0) << 16;
  e = (v & 0xfff00000) | 0xfffff;

  if ((v & 0x0f) == 1)
    {
      r->add_flags(Resource::F_width_64bit);
      v = c.read<l4_uint32_t>(Config::Pref_mem_base_hi);
      s |= l4_uint64_t(v) << 32;
      v = c.read<l4_uint32_t>(Config::Pref_mem_limit_hi);
      e |= l4_uint64_t(v) << 32;
    }

  r->alignment(0xfffff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();

  pref_mmio = r;
  r->validate();
  _host->add_resource_rq(r);

  v = c.read<l4_uint16_t>(Config::Io_base);
  s = (v & 0xf0) << 8;
  e = (v & 0xf000) | 0xfff;

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN2");
  r->alignment(0xfff);
  if (s < e)
    r->start_end(s, e);
  else
    r->set_empty();
  io = r;
  r->validate();
  _host->add_resource_rq(r);

  Dev::discover_resources(host);
}

class Pcie_downstream_port : public Bridge
{
private:
  bool _ari = false;

public:
  explicit Pcie_downstream_port(Hw::Device *host, Bridge_if *bridge,
                                Config_cache const &cfg)
  : Bridge(host, bridge, nullptr, cfg)
  {
  }

  bool ari_forwarding_enable() override
  {
    if (_ari)
      return true;

    Cap pcie = pcie_cap();
    l4_uint32_t v = pcie.read<l4_uint32_t>(0x24);
    if (v & (1UL << 5))
      {
        v = pcie.read<l4_uint16_t>(0x28);
        v |= (1UL << 5); // enable ARI forwarding
        pcie.write<l4_uint16_t>(0x28, v);

        _ari = true;
      }
    return _ari;
  }

  void discover_devices(Hw::Device *host_bus, Config_space *cfg,
                        Io_irq_pin::Msi_src *ext_msi) override
  {
    (void) ext_msi;
    assert (ext_msi == nullptr);

    Dev *d = discover_func(host_bus, cfg, nullptr, 0, 0);
    if (!d)
      return;

    Extended_cap ari = d->find_ext_cap(Ari_cap::Id);
    // we must check if we are the root complex or some ordinary PCIe BUS.
    if (ari_forwarding_enable() && !ari.Config::is_valid())
      ari = Extended_cap();

    while (ari.Config::is_valid())
      {
        l4_uint8_t next_func = ari.read<Ari_cap::Caps>().next_func();
        if (!next_func)
          return;

        d = discover_func(host_bus, cfg, nullptr, 0, next_func);
        if (!d)
          return;

        ari = d->find_ext_cap(Ari_cap::Id);
      }

    // look for functions in PCI style
    if (d->cfg.is_multi_function())
      for (int function = 1; function < 8; ++function)
        discover_func(host_bus, cfg, nullptr, 0, function);
  }
};

class Pcie_bridge : public Bridge
{
private:
  struct Secondary_msi_src : Io_irq_pin::Msi_src
  {
    cxx::H_list_t<Msi_mgr> _msi_mgrs;
    l4_uint8_t secondary;

    l4_uint64_t get_src_info(Msi_mgr *mgr) override
    {
      if (mgr)
        _msi_mgrs.add(mgr);

      return 0x80000 | (secondary << 8) | secondary;
    }
  };

  Secondary_msi_src _bus_msi_src;

public:
  explicit Pcie_bridge(Hw::Device *host, Bridge_if *bridge,
                       Config_cache const &cfg)
  : Bridge(host, bridge, nullptr, cfg)
  {}

  void check_bus_config() override
  {
    Bridge::check_bus_config();
    _bus_msi_src.secondary = num;
  }

  /// Devices downstream use the secondary Bus src-id
  Io_irq_pin::Msi_src *get_downstream_src_id() override
  {
    return &_bus_msi_src;
  }

  void discover_resources(Hw::Device *host) override
  {
    // using 'nullptr' triggers DMAR domains to use downstream_src_id().
    host->set_downstream_dma_domain(host->dma_domain_for(nullptr));
    Bridge::discover_resources(host);
  }

  void discover_bus(Hw::Device *host) override
  {
    Bridge_base::discover_bus(host, cfg.cfg_spc(), &_bus_msi_src);
    Dev::discover_bus(host);
  }
};

class Cardbus_bridge : public Generic_bridge
{
public:
  Cardbus_bridge(Hw::Device *host, Bridge_if *bridge,
                 Io_irq_pin::Msi_src *ext_msi,
                 Config_cache const &cfg)
  : Generic_bridge(host, bridge, ext_msi, cfg)
  {}

  void discover_resources(Hw::Device *host) override;
};

void
Cardbus_bridge::discover_resources(Hw::Device *host)
{
  if (flags.discovered())
    return;

  auto c = config();

  Resource *r = new Resource_provider(Resource::Mmio_res | Resource::F_can_move
                                      | Resource::F_can_resize);
  r->set_id("WIN0");
  r->start(c.read<l4_uint32_t>(Config::Cb_mem_base_0));
  r->end(c.read<l4_uint32_t>(Config::Cb_mem_limit_0));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Mmio_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN1");
  r->start(c.read<l4_uint32_t>(Config::Cb_mem_base_1));
  r->end(c.read<l4_uint32_t>(Config::Cb_mem_limit_1));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN2");
  r->start(c.read<l4_uint32_t>(Config::Cb_io_base_0));
  r->end(c.read<l4_uint32_t>(Config::Cb_io_limit_0));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN3");
  r->start(c.read<l4_uint32_t>(Config::Cb_io_base_1));
  r->end(c.read<l4_uint32_t>(Config::Cb_io_limit_1));
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  flags.discovered() = true;
}


static Generic_bridge *
create_pci_pci_bridge(Bridge_if *bridge, Io_irq_pin::Msi_src *ext_msi,
                      Config const &,
                      Config_cache const &cc,
                      Hw::Device *hw)
{
  if (cc.type() != 1)
    {
      d_printf(DBG_WARN,
               "ignoring PCI-PCI bridge with invalid header type: %u (%08x)\n",
               (unsigned)cc.type(), hw->adr());
      return nullptr;
    }

  Generic_bridge *b = nullptr;
  if (cc.pcie_cap)
    {
      switch (cc.pcie_type)
        {
        case 0x4: // Root Port of PCI Express Root Complex
        case 0x6: // Downstream Port of PCI Express Switch
          b = new Pcie_downstream_port(hw, bridge, cc);
          break;

        case 0x5: // Upstream Port of PCI Express Switch
          b = new Bridge(hw, bridge, nullptr, cc);
          break;

        case 0x7: // PCI Express to PCI/PCI-X bridge
          b = new Pcie_bridge(hw, bridge, cc);
          break;

        default:
          // all other ids are either no busses or
          // legacy PCI/PCI-X busses
          b = new Bridge(hw, bridge, ext_msi, cc);
          break;
        }
    }
  else
    b = new Bridge(hw, bridge, ext_msi, cc);

  b->check_bus_config();
  hw->set_name_if_empty("PCI-to-PCI bridge");
  return b;
}

static Generic_bridge *
create_pci_cardbus_bridge(Bridge_if *bridge,
                          Io_irq_pin::Msi_src *ext_msi,
                          Config const &,
                          Config_cache const &cc,
                          Hw::Device *hw)
{
  if (cc.type() != 2)
    {
      d_printf(DBG_WARN,
               "ignoring PCI-Cardbus bridge with invalid header type: %u (%08x)\n",
               (unsigned)cc.type(), hw->adr());
      return nullptr;
    }

  hw->set_name_if_empty("PCI-to-Cardbus bridge");
  auto b = new Cardbus_bridge(hw, bridge, ext_msi, cc);
  b->check_bus_config();
  return b;
}

static Dev *
create_pci_bridge(Bridge_if *bridge, Io_irq_pin::Msi_src *ext_msi,
                  Config const &cfg,
                  Config_cache const &cc,
                  Hw::Device *hw)
{
  switch (cc.sub_class())
    {
    case 0x4: return create_pci_pci_bridge(bridge, ext_msi, cfg, cc, hw);
    case 0x7: return create_pci_cardbus_bridge(bridge, ext_msi, cfg, cc, hw);
    default:
      if (cc.type() != 0)
        {
          d_printf(DBG_WARN,
                   "ignoring PCI bridge with invalid header type: %u (%08x)\n",
                   (unsigned)cc.type(), hw->adr());
          return nullptr;
        }

      hw->set_name_if_empty("PCI device");
      return new Dev(hw, bridge, ext_msi, cc);
    }
}

void
Bridge_base::discover_device(Hw::Device *host_bus, Config_space *cfg,
                             Io_irq_pin::Msi_src *ext_msi, int devnum)
{
  Dev *d = discover_func(host_bus, cfg, ext_msi, devnum, 0);
  if (!d)
    return;

  // look for functions in PCI style
  if (d->cfg.is_multi_function())
    for (int function = 1; function < 8; ++function)
      discover_func(host_bus, cfg, ext_msi, devnum, function);
}

Dev *
Bridge_base::discover_func(Hw::Device *host_bus, Config_space *cfg,
                           Io_irq_pin::Msi_src *ext_msi, int device, int function)
{
  Config config(Cfg_addr(num, device, function, 0), cfg);

  l4_uint32_t vendor = config.read<l4_uint32_t>(Config::Vendor);
  if ((vendor & 0xffff) == 0xffff)
    return nullptr;

#if 0
  // alex: hack disable serial IO cards for user apps
  if ((_class >> 16) == 0x0700)
    continue;
  // alex: hack end
#endif

  Hw::Device *child = host_bus->get_child_dev_adr(devfn(device, function), true);

  // skip device if we already were here
  if (Dev *dev = child->find_feature<Dev>())
    return dev;

  Config_cache cc;
  cc.fill(vendor, config);

  Dev *d;
  if (cc.base_class() == 0x6) // bridge
    {
      d = create_pci_bridge(this, ext_msi, config, cc, child);
      if (!d)
        return nullptr;
    }
  else
    {
      child->set_name_if_empty("PCI device");
      d = new Dev(child, this, ext_msi, cc);
    }

  child->add_feature(d);

  // discover the resources of the new PCI device
  // NOTE: we do this here to have all child resources discovered and
  // requested before the call to allocate_pending_resources in
  // hw_device.cc which can then recursively try to allocate resources
  // that were not preset
  d->discover_resources(child);

  Driver *drv = Driver::find(d);
  if (drv)
    drv->probe(d);

  // go down the PCI hierarchy recursively,
  // to assign bus numbers (if not yet assigned) the right way
  d->discover_bus(child);

  return d;
}

void
Bridge_base::discover_bus(Hw::Device *host, Config_space *cfg,
                          Io_irq_pin::Msi_src *ext_msi)
{
  Resource *r = host->resources()->find_if(Resource::is_irq_provider_s);

  if (!r)
    {
      r = new Pci::Irq_router_res<Pci::Pci_pci_bridge_irq_router_rs>();
      r->set_id("IRQR");
      host->add_resource_rq(r);
    }

  discover_devices(host, cfg, ext_msi);
}

void
Bridge_base::discover_devices(Hw::Device *host, Config_space *cfg,
                              Io_irq_pin::Msi_src *ext_msi)
{
  for (int device = 0; device <= 31; ++device)
    discover_device(host, cfg, ext_msi, device);
}

void
Bridge_base::dump(int) const
{
#if 0
  printf("PCI bus type: %d: ", bus_type);
  "bridge %04x:%02x:%02x.%x\n",
         b->num, 0, b->parent() ? (int)static_cast<Hw::Pci::Bus*>(b->parent())->num : 0,
         b->adr() >> 16, b->adr() & 0xffff);
#endif
#if 0
  //dump_res_rec(resources(), 0);

  for (iterator c = begin(0); c != end(); ++c)
    c->dump();
#endif
};

} }

