/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include "vpci.h"

namespace Vi {

// -----------------------------------------------------------------------
// Virtual PCI to PCI bridge
// -----------------------------------------------------------------------

class Pci_to_pci_bridge : public Pci_bridge, public Pci_virtual_dev
{
private:
  unsigned char _cfg_space[16*4];

public:
  struct Bridge_cfg
  {
    l4_uint8_t io_base;
    l4_uint8_t io_limit;
    l4_uint16_t sec_status;
    l4_uint16_t mem_base;
    l4_uint16_t mem_limit;
    l4_uint16_t pref_base;
    l4_uint16_t pref_limit;
    l4_uint32_t pref_base_upper;
    l4_uint32_t pref_limit_upper;
    l4_uint16_t io_base_upper;
    l4_uint16_t io_limit_upper;
  } __attribute__((packed));

  Pci_to_pci_bridge();
  int cfg_read(int reg, l4_uint32_t *v, Cfg_width o);
  int cfg_write(int reg, l4_uint32_t v, Cfg_width o);

  int irq_enable(Irq_info *irq)
  {
    irq->irq = -1;
    return -1;
  }

  Bridge_cfg *bridge_cfg() const
  { return reinterpret_cast<Bridge_cfg*>(_h + 7 * 4); }

  bool is_vpci_bridge() const { return true; }

  bool match_hw_feature(const Hw::Dev_feature*) const { return false; }
  int dispatch(l4_umword_t, l4_uint32_t, L4::Ipc::Iostream&)
  { return -L4_ENOSYS; }


};

}
