/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "vpci.h"
#include "vpci_pci_bridge.h"
#include "virt/vbus_factory.h"

#include <l4/vbus/vbus_pci-ops.h>

namespace Vi {

/**
 * \brief A virtual Host-to-PCI bridge.
 */
class Pci_vroot : public Pci_bridge, public Dev_feature
{
public:

  explicit Pci_vroot();

  int dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios) override;

  ~Pci_vroot() {}

  char const *hid() const override
  { return "PNP0A03"; }

  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

  l4_uint32_t interface_type() const override
  { return 1 << L4VBUS_INTERFACE_PCI; }

  Io_irq_pin::Msi_src *find_msi_src(Msi_src_info si) override;

private:
  Device *_host;

public:
  int cfg_read(L4::Ipc::Iostream &ios);
  int cfg_write(L4::Ipc::Iostream &ios);
  int irq_enable(L4::Ipc::Iostream &ios);
  bool match_hw_feature(const Hw::Dev_feature*) const override
  { return false; }
};

// -----------------------------------------------------------------------
// Virtual PCI root bridge with identity mapping of phys devices
// -----------------------------------------------------------------------


class Pci_vroot_id : public Pci_vroot
{
public:
  void add_child(Device *d);

};



// -------------------------------------------------------------------
// Virtual PCI Root bridge
// -------------------------------------------------------------------

Pci_vroot::Pci_vroot() : Pci_bridge()
{
  add_feature(this);
  _subordinate = 100;
}

Io_irq_pin::Msi_src *
Pci_vroot::find_msi_src(Msi_src_info si)
{
  if (si.svt() == 1) // exact match
    {
      Pci_dev *d = child_dev(si.bus(), si.dev(), si.fn());
      if (!d)
        return 0;
      return d->msi_src();
    }
  return 0;
}

int
Pci_vroot::cfg_read(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;
  l4_uint32_t reg;
  l4_uint32_t value = 0;
  l4_uint32_t width;

  ios >> bus >> devfn >> reg >> width;
  value = ~0U >> (32 - width);

  if (0)
    printf("vPCI root: cfg read: %02x:%02x.%1x: reg=%x w=%d\n",
           bus, devfn >> 16, devfn & 0xffff, reg, width);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    {
      ios << value;
      return L4_EOK;
    }

  Pci_dev *d = child_dev(bus, (devfn >> 16), (devfn & 0xffff));

  if (!d)
    {
      ios << value;
      return L4_EOK;
    }

  int res = d->cfg_read(reg, &value, Hw::Pci::cfg_w_to_o(width));

  if (0)
    printf("  res=%d\n", res);

  if (res < 0)
    return res;

  if (0)
    printf("  value=%x\n", value);

  ios << value;
  return L4_EOK;
}
int
Pci_vroot::irq_enable(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;

  ios >> bus >> devfn;

  if (0)
    printf("irq enable: %02x:%02x.%1x\n", bus, devfn >> 16, devfn & 0xffff);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    {
      ios << (int)-1;
      return L4_EOK;
    }

  Pci_dev *d = child_dev(bus, devfn >> 16, devfn & 0xffff);
  if (0)
    printf("dev = %p\n", d);
  if (!d)
    {
      ios << (int)-1;
      return L4_EOK;
    }

  Pci_dev::Irq_info info;
  int res = d->irq_enable(&info);
  if (res < 0)
    {
      ios << (int)-1;
      return res;
    }

  if (0)
    printf("  irq = %d\n", info.irq);
  ios << info.irq << info.trigger << info.polarity;
  return L4_EOK;
}

int
Pci_vroot::cfg_write(L4::Ipc::Iostream &ios)
{
  l4_uint32_t bus;
  l4_uint32_t devfn;
  l4_uint32_t reg;
  l4_uint32_t value;
  l4_uint32_t width;

  ios >> bus >> devfn >> reg >> value >> width;
  // printf("cfg write: %02x:%02x.%1x: reg=%x w=%x v=%08x\n", bus, devfn >> 16, devfn & 0xffff, reg, width, value);

  if ((devfn >> 16) >= 32 || (devfn & 0xffff) >= 8)
    return L4_EOK;

  Pci_dev *d = child_dev(bus, (devfn >> 16), (devfn & 0xffff));

  if (!d)
    return L4_EOK;

  return d->cfg_write(reg, value, Hw::Pci::cfg_w_to_o(width));
}

int
Pci_vroot::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  if (l4vbus_subinterface(func) != L4VBUS_INTERFACE_PCI)
    return -L4_ENOSYS;

  switch (func)
    {
    case L4vbus_pciroot_cfg_read: return cfg_read(ios);
    case L4vbus_pciroot_cfg_write: return cfg_write(ios);
    case L4vbus_pciroot_cfg_irq_enable: return irq_enable(ios);
    default: return -L4_ENOSYS;
    }
}


void
Pci_vroot_id::add_child(Device *d)
{
  Pci_dev *vp = d->find_feature<Pci_dev>();

  // hm, also here, drom non PCI devices
  if (!vp)
    return;

  if (Pci_proxy_dev *proxy = dynamic_cast<Pci_proxy_dev*>(vp))
    {
      Hw::Pci::Pci_pci_bridge_basic const *hw_br
	= dynamic_cast<Hw::Pci::Pci_pci_bridge_basic const *>(proxy->hwf()->bus());

      unsigned dn = proxy->hwf()->device_nr() & (Bus::Devs-1);
      unsigned fn = proxy->hwf()->function_nr() & (Dev::Fns-1);

      if (!hw_br)
	{
	  // MUST be a device on the root PCI bus
	  _bus.dev(dn)->fn(fn, vp);
	  Device::add_child(d);
	  return;
	}

      Pci_bridge *sw_br = find_bridge(hw_br->num);
      if (!sw_br)
	{
	  Pci_to_pci_bridge *b = new Pci_to_pci_bridge();
	  sw_br = b;
	  sw_br->primary(hw_br->pri);
	  sw_br->secondary(hw_br->num);
	  sw_br->subordinate(hw_br->subordinate);

	  unsigned dn = hw_br->device_nr() & (Bus::Devs-1);
	  unsigned fn = hw_br->function_nr() & (Dev::Fns-1);
	  _bus.dev(dn)->fn(fn, b);
	  Device::add_child(b);
	}

      sw_br->add_child_fixed(d, vp, dn, fn);
      return;
    }
  else
    {
      _bus.add_fn(vp);
      Device::add_child(d);
    }
}



static Dev_factory_t<Pci_vroot> __pci_root_factory("PCI_bus");
static Dev_factory_t<Pci_vroot_id> __pci_root_id_factory("PCI_bus_ident");

}
