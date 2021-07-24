/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */

#include <pci-root.h>
#include <vector>

#if defined(ARCH_x86) || defined(ARCH_amd64)

#include <l4/util/port_io.h>
#include <utils.h>

namespace Hw { namespace Pci {

int
Port_root_bridge::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width w)
{
  if (addr.reg() >= 0x100)
    {
      *value = ~0U;
      return 0;
    }

  Pthread_mutex_guard g(&_cfg_lock);
  l4util_out32((addr.to_compat_addr() | 0x80000000) & ~3UL, 0xcf8);

  switch (w)
    {
    case Cfg_byte:  *value = l4util_in8(0xcfc + addr.reg_offs(w)); break;
    case Cfg_short: *value = l4util_in16(0xcfc + addr.reg_offs(w)); break;
    case Cfg_long:  *value = l4util_in32(0xcfc); break;
    }
  return 0;
}

int
Port_root_bridge::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width w)
{
  if (addr.reg() >= 0x100)
    return 0;

  Pthread_mutex_guard g(&_cfg_lock);
  l4util_out32((addr.to_compat_addr() | 0x80000000) & ~3UL, 0xcf8);
  switch (w)
    {
    case Cfg_byte:  l4util_out8(value, 0xcfc + addr.reg_offs(w)); break;
    case Cfg_short: l4util_out16(value, 0xcfc + addr.reg_offs(w)); break;
    case Cfg_long:  l4util_out32(value, 0xcfc); break;
    }
  return 0;
}


int
Mmio_root_bridge::cfg_read(Cfg_addr addr, l4_uint32_t *value, Cfg_width w)
{
  switch (w)
    {
    case Cfg_byte:  *value = *(volatile l4_uint8_t  *)a(addr); break;
    case Cfg_short: *value = *(volatile l4_uint16_t *)a(addr); break;
    case Cfg_long:  *value = *(volatile l4_uint32_t *)a(addr); break;
    }
  return 0;
}

int
Mmio_root_bridge::cfg_write(Cfg_addr addr, l4_uint32_t value, Cfg_width w)
{
  switch (w)
    {
    case Cfg_byte:  *(volatile l4_uint8_t  *)a(addr) = value; break;
    case Cfg_short: *(volatile l4_uint16_t *)a(addr) = value; break;
    case Cfg_long:  *(volatile l4_uint32_t *)a(addr) = value; break;
    }

  return 0;
}


}}

#endif

namespace Hw { namespace Pci {

static std::vector<Root_bridge *> __pci_root_bridge;

void
Root_bridge::setup(Hw::Device *host)
{
  for (Resource_list::const_iterator i = host->resources()->begin();
       i != host->resources()->end(); ++i)
    if ((*i)->type() == Resource::Bus_res)
      num = subordinate = (*i)->start();

  Bridge_base::discover_bus(host, this);
}

Root_bridge *root_bridge(int segment)
{
  for (auto b: __pci_root_bridge)
    if (b->segment == segment)
      return b;

  return 0;
}

int
register_root_bridge(Root_bridge *b)
{
  for (auto x: __pci_root_bridge)
    if (x->segment == b->segment
        && x->num == b->num)
      return -L4_EEXIST;

  __pci_root_bridge.push_back(b);
  return 0;
}

Root_bridge *find_root_bridge(int segment, int bus)
{
  for (auto b: __pci_root_bridge)
    if (b->segment == segment
        && b->num == bus)
      return b;

  return 0;
}

} }
