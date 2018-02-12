/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "debug.h"
#include "hw_device.h"
#include "pci.h"
#include "main.h"

namespace {

inline
l4_uint32_t
pci_conf_addr0(l4_uint32_t bus, l4_uint32_t dev,
               l4_uint32_t fn, l4_uint32_t reg)
{ return (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3); }

class Pci_iomem_root_bridge : public Hw::Pci::Root_bridge, public Hw::Device
{
public:
  typedef Hw::Pci::Cfg_width Cfg_width;
  typedef Hw::Pci::Cfg_addr Cfg_addr;

  explicit Pci_iomem_root_bridge(int segment = 0, unsigned bus_nr = 0)
  : Hw::Pci::Root_bridge(segment, bus_nr, Pci_bus, this),
    _iobase_virt(0), _iobase_phys(-1),
    _dev_start(-1), _dev_end(-1), _iosize(0)
  {
    register_property("iobase", &_iobase_phys);
    register_property("iosize", &_iosize);
    register_property("dev_start", &_dev_start);
    register_property("dev_end", &_dev_end);
    register_property("int_a", &_int_map[0]);
    register_property("int_b", &_int_map[1]);
    register_property("int_c", &_int_map[2]);
    register_property("int_d", &_int_map[3]);
  }

  int cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width);
  int cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width);

  void discover_bus(Hw::Device *host);

  void init();

  int int_map(int i) const { return _int_map[i]; }

private:
  l4_addr_t _iobase_virt;
  Int_property _iobase_phys, _dev_start, _dev_end;
  Int_property _int_map[4];
  Int_property _iosize;
};

// Irq router that maps INTA-D to GSI
class Irq_router_rs : public Resource_space
{
public:
  bool request(Resource *parent, Device *, Resource *child, Device *cdev);
  bool alloc(Resource *, Device *, Resource *, Device *, bool)
  { return false; }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Irq_router_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Irq_router_rs\n");
    return false;
  }
};

void
Pci_iomem_root_bridge::init()
{
  if (_iobase_phys == -1)
    {
      d_printf(DBG_ERR, "ERROR: Hw::Pci::Root_bridge: 'iobase' not set.\n");
      return;
    }

  if (_iosize == 0)
    {
      d_printf(DBG_ERR, "ERROR: Hw::Pci::Root_bridge: 'iosize' not set.\n");
      return;
    }

  if (_dev_start == -1 || _dev_end == -1)
    {
      d_printf(DBG_ERR, "ERROR: Hw::Pci::Root_bridge: 'dev_start' and/or 'dev_end' not set.\n");
      return;
    }

  _iobase_virt = res_map_iomem(_iobase_phys, _iosize);
  if (!_iobase_virt)
    return;

  add_resource_rq(new Resource(Resource::Mmio_res,
                               _iobase_phys,
                               _iobase_phys + _iosize - 1));

  Resource *r = new Resource_provider(Resource::Mmio_res);
  r->alignment(0xfffff);
  r->start_end(_dev_start, _dev_end);
  add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res);
  r->start_end(0, 0xffff);
  add_resource_rq(r);

  add_resource_rq(new Hw::Pci::Irq_router_res<Irq_router_rs>());

  discover_bus(this);

  Hw::Device::init();
}

void
Pci_iomem_root_bridge::discover_bus(Hw::Device *host)
{
  if (!_iobase_virt)
    return;
  Hw::Pci::Root_bridge::discover_bus(host);
}

int
Pci_iomem_root_bridge::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width w)
{
  using namespace Hw;

  if (!_iobase_virt)
    return -1;

  l4_uint32_t volatile *a = (l4_uint32_t volatile *)(_iobase_virt + addr.to_compat_addr());
  switch (w)
    {
    case Pci::Cfg_byte:  *value = (l4_uint8_t)*a; break;
    case Pci::Cfg_short: *value = (l4_uint16_t)*a; break;
    case Pci::Cfg_long:  *value = (l4_uint32_t)*a; break;
    }

  d_printf(DBG_ALL, "Pci_iomem_root_bridge::cfg_read(%x, %x, %x, %x, %x, %d)\n",
           addr.bus(), addr.dev(), addr.fn(), addr.reg(), *value, w);

  return 0;
}

int
Pci_iomem_root_bridge::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width w)
{
  using namespace Hw;

  if (!_iobase_virt)
    return -1;

  d_printf(DBG_ALL, "Pci_iomem_root_bridge::cfg_write(%x, %x, %x, %x, %x, %d)\n",
           addr.bus(), addr.dev(), addr.fn(), addr.reg(), value, w);

  l4_addr_t a = _iobase_virt + addr.to_compat_addr();
  switch (w)
    {
    case Pci::Cfg_byte: *(volatile l4_uint8_t *)a = value; break;
    case Pci::Cfg_short: *(volatile l4_uint16_t *)a = value; break;
    case Pci::Cfg_long: *(volatile l4_uint32_t *)a = value; break;
    }
  return 0;
}

static Hw::Device_factory_t<Pci_iomem_root_bridge>
              __hw_pci_root_bridge_factory("Pci_iomem_root_bridge");

bool Irq_router_rs::request(Resource *parent, Device *pdev,
                            Resource *child, Device *cdev)
{
  Hw::Device *cd = dynamic_cast<Hw::Device*>(cdev);
  if (!cd)
    return false;

  if (child->start() > 3)
    return false;

  int i = (child->start() + (cd->adr() >> 16)) & 3;

  Pci_iomem_root_bridge *pd = dynamic_cast<Pci_iomem_root_bridge *>(pdev);
  if (!pd)
    return false;


  child->del_flags(Resource::F_relative);
  child->start(pd->int_map(i));
  child->del_flags(Resource::Irq_type_mask);
  child->add_flags(Resource::Irq_type_level_low);

  child->parent(parent);

  return true;
}
}
