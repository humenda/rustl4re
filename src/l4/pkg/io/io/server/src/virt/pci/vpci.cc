/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/vbus/vdevice-ops.h>
#include <l4/vbus/vbus_pci-ops.h>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <l4/cxx/exceptions>
#include <l4/io/pciids.h>
#include <l4/sys/err.h>

#include "debug.h"
#include "pci.h"
#include "vpci.h"
#include "virt/vbus_factory.h"

namespace Vi {

// -----------------------------------------------------------------------
// Pci_virtual_dev
// -----------------------------------------------------------------------

Pci_virtual_dev::Pci_virtual_dev()
{
  memset(&_h, 0, sizeof(_h));
}

int
Pci_virtual_dev::cfg_read(int reg, l4_uint32_t *v, Cfg_width order)
{
  reg >>= order;
  if ((unsigned)reg >= (_h_len >> order))
    return -L4_ERANGE;

#define RC(x) *v = *((Hw::Pci::Cfg_type<x>::Type const *)_h + reg); break
  *v = 0;
  switch (order)
    {
    case Hw::Pci::Cfg_byte: RC(Hw::Pci::Cfg_byte);
    case Hw::Pci::Cfg_short: RC(Hw::Pci::Cfg_short);
    case Hw::Pci::Cfg_long: RC(Hw::Pci::Cfg_long);
    }

  return 0;
#undef RC
}

int
Pci_virtual_dev::cfg_write(int reg, l4_uint32_t v, Cfg_width order)
{
  switch (reg & ~3)
    {
    case 0x4: // status is RO
      v &= 0x0000ffff << (reg & (3 >> order));
      break;

    default:
      break;
    }

  reg >>= order;
  if ((unsigned)reg >= (_h_len >> order))
    return -L4_ERANGE;

#define RC(x) *((Hw::Pci::Cfg_type<x>::Type *)_h + reg) = v; break
  switch (order)
    {
    case Hw::Pci::Cfg_byte: RC(Hw::Pci::Cfg_byte);
    case Hw::Pci::Cfg_short: RC(Hw::Pci::Cfg_short);
    case Hw::Pci::Cfg_long: RC(Hw::Pci::Cfg_long);
    }

  return 0;
#undef RC
}

// -----------------------------------------------------------------------
// Pci_dev_feature
// -----------------------------------------------------------------------



int
Pci_dev_feature::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream& ios)
{
  if (l4vbus_subinterface(func) != L4VBUS_INTERFACE_PCIDEV)
    return -L4_ENOSYS;

  l4_uint32_t reg;
  l4_uint32_t value = 0;
  l4_uint32_t width;
  Pci_dev::Irq_info info;
  int res;

  switch (func)
    {
    case L4vbus_pcidev_cfg_read:
      ios >> reg >> width;
      res = cfg_read(reg, &value, Hw::Pci::cfg_w_to_o(width));
      if (res < 0)
        return res;
      ios << value;
      return L4_EOK;
    case L4vbus_pcidev_cfg_write:
      ios >> reg >> value >> width;
      return cfg_write(reg, value, Hw::Pci::cfg_w_to_o(width));
    case L4vbus_pcidev_cfg_irq_enable:
      res = irq_enable(&info);
      if (res < 0)
        return res;
      ios << info.irq << info.trigger << info.polarity;
      return L4_EOK;
    default: return -L4_ENOSYS;
    }
}




// -----------------------------------------------------------------------
// Pci_proxy_dev
// -----------------------------------------------------------------------

bool
Pci_proxy_dev::scan_pci_caps()
{
  l4_uint8_t pci_cap;
  _hwf->cfg_read(Hw::Pci::Config::Capability_ptr, &pci_cap);
  bool is_pci_express = false;
  while (pci_cap)
    {
      l4_uint16_t cap;
      _hwf->cfg_read(pci_cap, &cap);
      switch (cap & 0xff)
        {
        case Hw::Pci::Cap::Pcie:
          is_pci_express = true;
          /* fall through */
        case Hw::Pci::Cap::Pcix:
        case Hw::Pci::Cap::Msi:
        case Hw::Pci::Cap::Msi_x:
        case Hw::Pci::Cap::Vndr:
        default:
          add_pci_cap(new Pci_proxy_cap(_hwf, pci_cap));
          break;
        }

      pci_cap = cap >> 8;
    }
  return is_pci_express;
}

void
Pci_proxy_dev::scan_pcie_caps()
{
  l4_uint16_t offset = 0x100;
  l4_uint32_t c;
  for (;;)
    {
      _hwf->cfg_read(offset, &c);
      if (offset == 0x100 && ((c & 0xffff) == 0xffff))
        return;

      add_pcie_cap(new Pcie_proxy_cap(_hwf, c, offset, offset));

      offset = c >> 20;
      if (!offset)
        break;
    }

  assert (!_pcie_caps || _pcie_caps->offset() == 0x100);
#if 0
  if (_pcie_caps && _pcie_caps->offset() != 0x100)
    _pcie_caps->set_offset(0x100);
#endif
}

Pci_proxy_dev::Pci_proxy_dev(Hw::Pci::If *hwf)
: _hwf(hwf), _rom(0)
{
  assert (hwf);
  for (int i = 0; i < 6; ++i)
    {
      Resource *r = _hwf->bar(i);

      if (!r)
	{
	  _vbars[i] = 0;
	  continue;
	}

      if (_hwf->is_64bit_high_bar(i))
	{
	  _vbars[i] = l4_uint64_t(r->start()) >> 32;
	}
      else
	{
	  _vbars[i] = r->start();
	  if (r->type() == Resource::Io_res)
	    _vbars[i] |= 1;

	  if (r->is_64bit())
	    _vbars[i] |= 4;

	  if (r->prefetchable())
	    _vbars[i] |= 8;

	}

      //printf("  bar: %d = %08x\n", i, _vbars[i]);
    }

  if (_hwf->rom())
    _rom = _hwf->rom()->start();

  if (scan_pci_caps())
    scan_pcie_caps();
}

int
Pci_proxy_dev::irq_enable(Irq_info *irq)
{
  for (Resource_list::const_iterator i = host()->resources()->begin();
       i != host()->resources()->end(); ++i)
    {
      Resource *res = *i;

      if (!res->disabled() && res->type() == Resource::Irq_res)
	{
	  irq->irq = res->start();
          irq->trigger = !res->irq_is_level_triggered();
	  irq->polarity = res->irq_is_low_polarity();
	  d_printf(DBG_DEBUG, "Enable IRQ: irq=%d trg=%x pol=%x\n",
                   irq->irq, irq->trigger, irq->polarity);
	  if (dlevel(DBG_DEBUG2))
	    dump();
	  return 0;
	}
    }
  return -L4_EINVAL;
}



l4_uint32_t
Pci_proxy_dev::read_bar(int bar)
{
  // d_printf(DBG_ALL, "   read bar[%x]: %08x\n", bar, _vbars[bar]);
  return _vbars[bar];
}

void
Pci_proxy_dev::write_bar(int bar, l4_uint32_t v)
{
  Hw::Pci::If *p = _hwf;

  Resource *r = p->bar(bar);
  if (!r)
    return;

  // printf("  write bar[%x]: %llx-%llx...\n", bar, r->abs_start(), r->abs_end());
  l4_uint64_t size_mask = r->alignment();

  if (r->type() == Resource::Io_res)
    size_mask |= 0xffff0000;

  if (p->is_64bit_high_bar(bar))
    size_mask >>= 32;

  _vbars[bar] = (_vbars[bar] & size_mask) | (v & ~size_mask);

  // printf("    bar=%lx\n", _vbars[bar]);
}

void
Pci_proxy_dev::write_rom(l4_uint32_t v)
{
  Hw::Pci::If *p = _hwf;

  // printf("write rom bar %x %p\n", v, _dev->rom());
  Resource *r = p->rom();
  if (!r)
    return;

  l4_uint64_t size_mask = r->alignment();

  _rom = (_rom & size_mask) | (v & (~size_mask | 1));

  p->cfg_write(0x30, (r->start() & ~1U) | (v & 1), Hw::Pci::Cfg_long);
}

Pci_capability *
Pci_proxy_dev::find_pci_cap(unsigned offset) const
{
  if (!_pci_caps || offset < 0x3c)
    return 0;

  for (Pci_capability *c = _pci_caps; c; c = c->next())
    if (c->is_inside(offset))
      return c;

  return 0;
}

void
Pci_proxy_dev::add_pci_cap(Pci_capability *c)
{
  Pci_capability **i;
  for (i = &_pci_caps; *i; i = &(*i)->next())
    if ((*i)->offset() > c->offset())
      break;

  c->next() = *i;
  *i = c;
}

Pcie_capability *
Pci_proxy_dev::find_pcie_cap(unsigned offset) const
{
  if (!_pcie_caps || offset < 0x100)
    return 0;

  for (Pcie_capability *c = _pcie_caps; c; c = c->next())
    if (c->is_inside(offset))
      return c;

  return 0;
}

void
Pci_proxy_dev::add_pcie_cap(Pcie_capability *c)
{
  Pcie_capability **i;
  for (i = &_pcie_caps; *i; i = &(*i)->next())
    if ((*i)->offset() > c->offset())
      break;

  c->next() = *i;
  *i = c;
}

int
Pci_proxy_dev::cfg_read(int reg, l4_uint32_t *v, Cfg_width order)
{
  Hw::Pci::If *p = _hwf;

  l4_uint32_t buf;

  l4_uint32_t const *r = &buf;
  reg &= ~0U << order;
  int dw_reg = reg & ~3;

  if (Pci_capability *cap = find_pci_cap(dw_reg))
    return cap->cfg_read(reg, v, order);

  if (Pcie_capability *cap = find_pcie_cap(dw_reg))
    return cap->cfg_read(reg, v, order);

  switch (dw_reg)
    {
    case 0x00: buf = p->vendor_device_ids(); break;
    case 0x08: buf = p->class_rev(); break;
    case 0x04: buf = p->checked_cmd_read(); break;
    /* simulate multi function on hdr type */
    case 0x0c: p->cfg_read(dw_reg, &buf); buf |= 0x00800000; break;
    case 0x10: /* bars 0 to 5 */
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24: buf = read_bar((dw_reg - 0x10) / 4); break;
    case 0x2c: buf = p->subsys_vendor_ids(); break;
    case 0x30: buf = read_rom(); break;
    case 0x34: /* CAPS */
               if (_pci_caps)
                 buf = _pci_caps->offset();
               else
                 buf = 0;
               break;
    case 0x38: buf = 0; break; /* PCI 3.0 reserved */

    case 0x100: /* empty PCIe cap */
               buf = 0xffff; break;
    default:   if (0) printf("pass unknown PCI cfg read for device %s: %x\n",
                             host()->name(), reg);
               /* fall through */
    case 0x28:
    case 0x3c:
               /* pass trough the rest ... */
               p->cfg_read(dw_reg, &buf);
               break;
    }

  unsigned mask = ~0U >> (32 - (8U << order));
  *v = (*r >> ((reg & 3) *8)) & mask;
  return L4_EOK;
}


void
Pci_proxy_dev::_do_cmd_write(unsigned mask,unsigned value)
{
  _hwf->checked_cmd_write(mask, value);
}

int
Pci_proxy_dev::_do_status_cmd_write(l4_uint32_t mask, l4_uint32_t value)
{
  if (mask & 0xffff)
    _do_cmd_write(mask & 0xffff, value & 0xffff);

  // status register has 'write 1 to clear' semantics so we can always write
  // 16bit with the bits masked that we do not want to affect.
  if (mask & value & 0xffff0000)
    _hwf->cfg_write(Pci::Config::Status, (value & mask) >> 16, Hw::Pci::Cfg_short);

  return 0;
}

int
Pci_proxy_dev::_do_rom_bar_write(l4_uint32_t mask, l4_uint32_t value)
{
  l4_uint32_t b = read_rom();

  if ((value & mask & 1) && !(b & mask & 1) && !_hwf->enable_rom())
    return 0;  // failed to enable

  b &= ~mask;
  b |= value & mask;
  write_rom(b);
  return 0;
}


int
Pci_proxy_dev::cfg_write(int reg, l4_uint32_t v, Cfg_width order)
{
  Hw::Pci::If *p = _hwf;

  reg &= ~0U << order;
  l4_uint32_t const offset_32 = reg & 3;
  l4_uint32_t const mask_32 = (~0U >> (32 - (8U << order))) << (offset_32 * 8);
  l4_uint32_t const value_32 = v << (offset_32 * 8);

  if (Pci_capability *cap = find_pci_cap(reg & ~3))
    return cap->cfg_write(reg, v, order);

  if (Pcie_capability *cap = find_pcie_cap(reg & ~3))
    return cap->cfg_write(reg, v, order);

  switch (reg & ~3)
    {
    case 0x00: return 0;
    case 0x08: return 0;
    case 0x04: return _do_status_cmd_write(mask_32, value_32);
    /* simulate multi function on hdr type */
    case 0x10: /* bars 0 to 5 */
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24:
      {
        l4_uint32_t b = read_bar(reg / 4 - 4);
        b &= ~mask_32;
        b |= value_32 & mask_32;
        write_bar(reg / 4 - 4, b);
        return 0;
      }
    case Hw::Pci::Config::Subsys_vendor:  return 0;
    case Hw::Pci::Config::Rom_address:    return _do_rom_bar_write(mask_32, value_32);
    case Hw::Pci::Config::Capability_ptr: return 0;
    case 0x38: return 0;
    /* pass trough the rest ... */
    default:   if (0) printf("pass unknown PCI cfg write for device %s: %x\n",
                             host()->name(), reg);
               /* fall through */
    case 0x0c:
    case 0x28:
    case 0x3c: return p->cfg_write(reg, v, order);
    }
}

void
Pci_proxy_dev::dump() const
{
  Hw::Pci::If *p = _hwf;

  printf("       %04x:%02x:%02x.%x:\n",
         0, p->bus_nr(), _hwf->device_nr(), _hwf->function_nr());
#if 0
#ifdef CONFIG_L4IO_PCIID_DB
  char buf[130];
  libpciids_name_device(buf, sizeof(buf), _dev->vendor(), _dev->device());
  printf("              %s\n", buf);
#endif
#endif
}

Pci_dev::Msi_src *
Pci_proxy_dev::msi_src() const
{
  assert (hwf());
  return hwf()->get_msi_src();
}

// -----------------------------------------------------------------------
// Virtual PCI dummy device
// -----------------------------------------------------------------------

class Pci_dummy : public Pci_virtual_dev, public Device
{
private:
  unsigned char _cfg_space[4*4];

public:
  int irq_enable(Irq_info *irq)
  {
    irq->irq = -1;
    return -1;
  }

  Pci_dummy()
  {
    add_feature(this);
    _h = &_cfg_space[0];
    _h_len = sizeof(_cfg_space);
    cfg_hdr()->hdr_type = 0x80;
    cfg_hdr()->vendor_device = 0x02000400;
    cfg_hdr()->status = 0;
    cfg_hdr()->class_rev = 0x36440000;
    cfg_hdr()->cmd = 0x0;
  }

  bool match_hw_feature(const Hw::Dev_feature*) const { return false; }
  void set_host(Device *d) { _host = d; }
  Device *host() const { return _host; }

private:
  Device *_host;
};


// ----------------------------------------------------------------------
// Basic virtual PCI bridge functionality
// ----------------------------------------------------------------------

Pci_bridge::Dev::Dev()
{
  memset(_fns, 0, sizeof(_fns));
}

void
Pci_bridge::Dev::add_fn(Pci_dev *f)
{
  for (unsigned i = 0; i < sizeof(_fns)/sizeof(_fns[0]); ++i)
    {
      if (!_fns[i])
	{
	  _fns[i] = f;
	  return;
	}
    }
}

void
Pci_bridge::Dev::sort_fns()
{
  // disabled sorting because the relation of two functions is questionable
#if 0
  unsigned n;
  for (n = 0; n < sizeof(_fns)/sizeof(_fns[0]) && _fns[n]; ++n)
    ;

  if (n < 2)
    return;

  bool exchg = false;

  do
    {
      exchg = false;
      for (unsigned i = 0; i < n - 1; ++i)
	{
	  if (_fns[i]->dev()->function_nr() > _fns[i+1]->dev()->function_nr())
	    {
	      Pci_dev *t = _fns[i];
	      _fns[i] = _fns[i+1];
	      _fns[i+1] = t;
	      exchg = true;
	    }
	}
      n -= 1;
    }
  while (exchg && n >= 1);
#endif
}

void
Pci_bridge::Bus::add_fn(Pci_dev *pd, int slot)
{
  if (slot >= 0)
    {
      _devs[slot].add_fn(pd);
      _devs[slot].sort_fns();
      return;
    }

  for (unsigned d = 0; d < Devs && !_devs[d].empty(); ++d)
    if (_devs[d].cmp(pd))
      {
	_devs[d].add_fn(pd);
	_devs[d].sort_fns();
	return;
      }

  for (unsigned d = 0; d < Devs; ++d)
    if (_devs[d].empty())
      {
	_devs[d].add_fn(pd);
	return;
      }
}

void
Pci_bridge::add_child(Device *d)
{
  Pci_dev *vp = d->find_feature<Pci_dev>();

  // hm, we do currently not add non PCI devices.
  if (!vp)
    return;

  _bus.add_fn(vp);
  Device::add_child(d);
}


void
Pci_bridge::add_child_fixed(Device *d, Pci_dev *vp, unsigned dn, unsigned fn)
{
  _bus.dev(dn)->fn(fn, vp);
  Device::add_child(d);
}


Pci_bridge *
Pci_bridge::find_bridge(unsigned bus)
{
  // printf("PCI[%p]: look for bridge for bus %x %02x %02x\n", this, bus, _subordinate, _secondary);
  if (bus == _secondary)
    return this;

  if (bus < _secondary || bus > _subordinate)
    return 0;

  for (unsigned d = 0; d < Bus::Devs; ++d)
    for (unsigned f = 0; f < Dev::Fns; ++f)
      {
	Pci_dev *p = _bus.dev(d)->fn(f);
	if (!p)
	  break;

	Pci_bridge *b = dynamic_cast<Pci_bridge*>(p);
	if (b && (b = b->find_bridge(bus)))
	  return b;
      }

  return 0;
}


Pci_dev *
Pci_bridge::child_dev(unsigned bus, unsigned char dev, unsigned char fn)
{
  Pci_bridge *b = find_bridge(bus);
  if (!b)
    return 0;

  if (dev >= Bus::Devs || fn >= Dev::Fns)
    return 0;

  return b->_bus.dev(dev)->fn(fn);
}

void
Pci_bridge::setup_bus()
{
  for (unsigned d = 0; d < Bus::Devs; ++d)
    for (unsigned f = 0; f < Dev::Fns; ++f)
      {
	Pci_dev *p = _bus.dev(d)->fn(f);
	if (!p)
	  break;

	Pci_bridge *b = dynamic_cast<Pci_bridge*>(p);
	if (b)
	  {
	    b->_primary = _secondary;
	    if (b->_secondary <= _secondary)
	      {
	        b->_secondary = ++_subordinate;
		b->_subordinate = b->_secondary;
	      }

	    b->setup_bus();

	    if (_subordinate < b->_subordinate)
	      _subordinate = b->_subordinate;
	  }
      }
}

void
Pci_bridge::finalize_setup()
{
  for (unsigned dn = 0; dn < Bus::Devs; ++dn)
    {
      if (!_bus.dev(dn)->empty())
	continue;

      for (unsigned fn = 0; fn < Dev::Fns; ++fn)
	if (_bus.dev(dn)->fn(fn))
	  {
	    Pci_dummy *dummy = new Pci_dummy();
	    _bus.dev(dn)->fn(0, dummy);
	    Device::add_child(dummy);
	    break;
	  }
    }
#if 0
  for (VDevice *c = dynamic_cast<VDevice*>(children()); c; c = c->next())
    c->finalize_setup();
#endif
}

namespace {
  static Feature_factory_t<Pci_proxy_dev, Hw::Pci::Dev> __pci_f_factory1;
  static Dev_factory_t<Pci_dummy> __pci_dummy_factory("PCI_dummy_device");
}

}

