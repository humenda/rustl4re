/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "vpci_pci_bridge.h"
#include "virt/vbus_factory.h"

namespace Vi {

Pci_to_pci_bridge::Pci_to_pci_bridge()
{
  add_feature(this);
  _h = &_cfg_space[0];
  _h_len = sizeof(_cfg_space);
  cfg_hdr()->hdr_type = 1;
  cfg_hdr()->vendor_device = 0x02000400;
  cfg_hdr()->status = 0;
  cfg_hdr()->class_rev = 0x06040000;
  cfg_hdr()->cmd = 0x3;

  cfg_write(Pci::Config::Primary, 0x00010100, Hw::Pci::Cfg_long);

  Bridge_cfg *bc = bridge_cfg();
  bc->io_base = 0xc0;      bc->io_base_upper = 0;
  bc->io_limit = 0xf0;  bc->io_limit_upper = 0;
  bc->mem_base = 0xa000;
  bc->mem_limit = 0xeff0;
  bc->pref_base = 0xf001; bc->pref_base_upper = 0;
  bc->pref_limit = 0xfff1; bc->pref_limit_upper = 0; //0xffffffff;
}


int
Pci_to_pci_bridge::cfg_read(int reg, l4_uint32_t *v, Cfg_width o)
{
  unsigned mask = ~0U >> (32 - (1U << (o + 3)));
  switch (reg & ~3)
    {
    case 0x18: // bus config stuff
      *v = (_bus_config >> ((reg & 3)*8)) & mask;
      return L4_EOK;

    case 0x10: // We have no BARs
    case 0x14:
    case 0x38:
      *v = 0;
      return L4_EOK;

    default:
      break;
    }

  return Pci_virtual_dev::cfg_read(reg, v, o);
}


int
Pci_to_pci_bridge::cfg_write(int reg, l4_uint32_t v, Cfg_width o)
{
  unsigned mask = (~0U >> (32 - (1U << (o + 3)))) << ((reg & 3) * 8);
  switch (reg & ~3)
    {
    case 0x18:
      _bus_config = (_bus_config & ~mask) | ((v << ((reg & 3)*8)) & mask);
      return L4_EOK;

    case 0x10: // We have no BARs
    case 0x14:
    case 0x38:
      return L4_EOK;

    default:
      break;
    }
  int r = Pci_virtual_dev::cfg_write(reg, v, o);

  switch (reg & ~3)
    {
    case 0x1c:
      *(l4_uint32_t*)(_h + 0x1c) &= 0x0000f000;
      break;
    case 0x24:
      *(l4_uint32_t*)(_h + 0x24) &= 0xfff0fff0;
      *(l4_uint32_t*)(_h + 0x24) |= 0x00010001;
      break;
    case 0x22:
      *(l4_uint32_t*)(_h + 0x22) &= 0xfff0fff0;
      break;
    }

  return r;
}


static Dev_factory_t<Pci_to_pci_bridge> __pci_to_pci_factory("PCI_PCI_bridge");

}
