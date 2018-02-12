/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/sys/types.h>
#include <l4/cxx/string>
#include <l4/cxx/minmax>
#include <l4/io/pciids.h>

#include <l4/util/util.h>

#include <cassert>
#include <cstdio>
#include <typeinfo>
#include <map>
#include <vector>

#include "debug.h"
#include "main.h"
#include "pci.h"
#include "phys_space.h"
#include "cfg.h"
#include "../utils.h"

namespace Hw { namespace Pci {

static std::vector<Root_bridge *> __pci_root_bridge;

class Pci_cardbus_bridge : public Pci_pci_bridge_basic
{
public:
  Pci_cardbus_bridge(Hw::Device *host, Bus *bus, l4_uint8_t hdr_type)
  : Pci_pci_bridge_basic(host, bus, hdr_type)
  {}

  void discover_resources(Hw::Device *host);
};

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

}}

#if defined(ARCH_x86) || defined(ARCH_amd64)

#include <l4/util/port_io.h>

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

namespace {

typedef std::map<l4_uint32_t, Driver *> Drv_list;

static Drv_list &driver_for_vendor_device()
{
  static Drv_list l;
  return l;
}

static Drv_list &driver_for_class()
{
  static Drv_list l;
  return l;
}

static inline l4_uint32_t
devfn(unsigned dev, unsigned fn)
{ return (dev << 16) | fn; }

} // end of local stuff


bool
Bus::discover_bus(Hw::Device *host)
{
  if (!irq_router)
    {
      Resource *r = new Pci::Irq_router_res<Pci::Pci_pci_bridge_irq_router_rs>();
      r->set_id("IRQR");
      host->add_resource_rq(r);
      irq_router = r;
    }

  bool found = false;

  int device = 0;
  for (device = 0; device < 32; ++device)
    {
      int funcs = 1;
      for (int function = 0; function < funcs; ++function)
	{
          Config config(Cfg_addr(num, device, function, 0), this);
	  l4_uint32_t vendor = config.read<l4_uint32_t>(Config::Vendor);
	  if ((vendor & 0xffff) == 0xffff)
            {
              if (function == 0)
                break;
              else
	        continue;
            }

          l4_uint32_t _class = config.read<l4_uint32_t>(Config::Class_rev);

          l4_uint8_t hdr_type = config.read<l4_uint8_t>(Config::Header_type);

          if (hdr_type & 0x80)
            funcs = 8;

	  Hw::Device *child = host->get_child_dev_adr(devfn(device, function), true);

          // skip device if we already were here
          if (child->find_feature<Dev>())
            continue;

          found |= true;

	  Dev *d;
	  if ((_class >> 16) == 0x0604 || (_class >> 16) == 0x0607)
	    {
	      Pci_pci_bridge_basic *b = 0;
	      if ((hdr_type & 0x7f) == 1)
		b = new Pci_pci_bridge(child, this, hdr_type);
	      else if((hdr_type & 0x7f) == 2)
		b = new Pci_cardbus_bridge(child, this, hdr_type);

	      l4_uint32_t buses;
	      bool reassign_buses = false;

	      buses = config.read<l4_uint32_t>(Config::Primary);

	      if ((buses & 0xff) != num
	          || ((buses >> 8) & 0xff) <= num)
		reassign_buses = true;

	      if (reassign_buses)
		{
		  unsigned new_so = subordinate + 1;
		  b->pri = num;
		  b->num = new_so;
		  b->subordinate = b->num;

		  buses = (buses & 0xff000000)
		    | ((l4_uint32_t)(b->pri))
		    | ((l4_uint32_t)(b->num) << 8)
		    | ((l4_uint32_t)(b->subordinate) << 16);

		  config.write(Config::Primary, buses);
		  increase_subordinate(new_so);
		}
	      else
		{
		  b->pri = buses & 0xff;
		  b->num = (buses >> 8) & 0xff;
		  b->subordinate = (buses >> 16) & 0xff;
		}

	      d = b;
	    }
	  else
	    d = new Dev(child, this, hdr_type);

	  child->add_feature(d);

	  d->vendor_device = vendor;
	  d->cls_rev = _class;

          // discover the resources of the new PCI device
          // NOTE: we do this here to have all child resources discovered and
          // requested before the call to allocate_pending_resources in
          // hw_device.cc which can then recursively try to allocate resources
          // that were not preset
          d->discover_resources(child);

          // go down the PCI hierarchy recursively,
          // to assign bus numbers (if not yet assigned) the right way
          d->discover_bus(child);
	}
    }
  return found;
}


l4_uint32_t
Dev::vendor_device_ids() const
{ return vendor_device; }

l4_uint32_t
Dev::class_rev() const
{ return cls_rev; }

l4_uint32_t
Dev::subsys_vendor_ids() const
{ return subsys_ids; }

unsigned
Dev::bus_nr() const
{ return _bus->num; }

l4_uint32_t
Dev::checked_cmd_read()
{
  l4_uint32_t cs = cfg_read<l4_uint32_t>(Config::Command);
  if (_transp_msi)
    return _transp_msi->filter_cmd_read(cs);

  return cs;
}

l4_uint16_t
Dev::checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd)
{
  l4_uint16_t ocmd = cfg_read<l4_uint16_t>(Config::Command);
  l4_uint16_t ncmd = (ocmd & ~mask) | (cmd & mask);

  if (_transp_msi)
    ncmd = _transp_msi->filter_cmd_write(ncmd, ocmd);

  if (ncmd == ocmd)
    return ocmd;

  unsigned enable_decoders = recheck_bars(ncmd & ~ocmd & 3);
  if ((ncmd & ~ocmd & 3) && !enable_decoders)
    {
      d_printf(DBG_WARN, "warning: could not set bars, disable decoders\n");
      ncmd &= (~3U | enable_decoders);
    }

  cfg_write<l4_uint16_t>(Pci::Config::Command, ncmd);
  return ncmd;
}

l4_uint32_t
Dev::recheck_bars(unsigned enable_decoders)
{
  if (!enable_decoders)
    return 0;

  unsigned next_bar;
  for (unsigned i = 0; i < 6; i += next_bar)
    {
      l4_uint32_t const bar_addr = Pci::Config::Bar_0 + i * 4;
      l4_uint32_t bar = cfg_read<l4_uint32_t>(bar_addr);
      bool const is_io_bar = bar & 1;
      bool const is_64bit  = (bar & 0x7) == 0x4;
      next_bar = is_64bit ? 2 : 1;
      unsigned const decoder = is_io_bar ? 1 : 2;
      l4_uint32_t const mask = is_io_bar ? l4_uint32_t(~3) : l4_uint32_t(~0xf);

      Resource *r = _bars[i];

      if (!r)
        continue;

      if ((bar & 2)
          || (is_io_bar && r->type() != Resource::Io_res)
          || (!is_io_bar && r->type() != Resource::Mmio_res))
        {
          // invalid BAR, the device is in a broken state, so disable
          // all decoders
          enable_decoders &= ~3;
          // skip the other BARs as decoders would be disabled
          // anyways
          break;
        }

      if (r->disabled())
        {
          // there are resource conflicts, we do not allow to enable them
          enable_decoders &= ~decoder;
          continue;
        }

      l4_uint64_t addr = 0;
      if (is_64bit)
        {
          addr = cfg_read<l4_uint32_t>(bar_addr + 4);
          addr <<= 32;
        }

      addr |= bar & mask;

      if (r->start() == addr)
        continue; // everything is fine

      addr = r->start();
      if (is_64bit)
        {
          cfg_write<l4_uint32_t>(bar_addr + 4, addr >> 32);
          if (cfg_read<l4_uint32_t>(bar_addr + 4) != addr >> 32)
            {
              d_printf(DBG_ERR, "error: PCI BAR refused write: bar=%d\n", i);
              enable_decoders &= ~decoder;
              continue;
            }
        }

      bar = (bar & ~mask) | (addr & mask);

      cfg_write(bar_addr, bar);
      if (cfg_read<l4_uint32_t>(bar_addr) != bar)
        {
          d_printf(DBG_ERR, "error: PCI BAR refused write: bar=%d\n", i);
          enable_decoders &= ~decoder;
        }
    }

  if (!enable_decoders)
    {
      d_printf(DBG_ERR, "error: PCI BARs could not be set correctly, need to disable decoders: %x\n",
               enable_decoders);
    }

  return enable_decoders;
}

bool
Dev::enable_rom()
{
  if (!_rom)
    return false;

  l4_uint32_t r = cfg_read<l4_uint32_t>(Config::Rom_address);

  // ROM always enabled
  if (r & 1)
    return true;

  // No rom at all
  if (r & 2)
    return false;

  l4_uint32_t rom = _rom->start();
  if ((r & ~0x3ffUL) == rom)
    return true;

  r = rom | 1;
  cfg_write(Config::Rom_address, r);
  return cfg_read<l4_uint32_t>(Config::Rom_address) == r;
}

unsigned
Dev::disable_decoders()
{
  l4_uint16_t v = 0;
  // disable decoders, and mask IRQs
  cfg_read(Config::Command, &v);
  cfg_write(Config::Command, (v & ~3) | (1<<10));
  return v & 0xff;
}

void
Dev::restore_decoders(unsigned cmd)
{
  cfg_write(Config::Command, cmd, Cfg_short);
}

Cap
Dev::find_pci_cap(unsigned char id)
{
  l4_uint32_t cap_ptr;

  switch (hdr_type & 0x7f)
    {
    case 0:
    case 1:
      cap_ptr = Config::Capability_ptr;
      break;
    case 2:
      cap_ptr = Config::Cb_capability_ptr;
      break;
    default:
      d_printf(DBG_WARN, "warning: %s: unknown hdr_type: %u\n",
                         __func__, hdr_type);
      return Cap();
    }

  l4_uint8_t o;
  cfg_read(cap_ptr, &o);
  if (o == 0)
    return Cap();

  for (Cap c = Cap(cfg_addr(o), bus()); c.is_valid(); c = c.next())
    if (c.id() == id)
      return c;

  return Cap();
}

int
Dev::discover_bar(int bar)
{
  l4_uint32_t v, x;

  _bars[bar] = 0;
  int r = Config::Bar_0 + bar * 4;
  l4_uint16_t cmd = disable_decoders();

  cfg_read(r, &v, Cfg_long);
  cfg_write(r, ~0U, Cfg_long);
  cfg_read(r, &x, Cfg_long);
  cfg_write(r, v, Cfg_long);

  restore_decoders(cmd);

  if (!x)
    return bar + 1;

  unsigned io_flags = (cmd & CC_io) ? 0 : Resource::F_disabled;
  unsigned mem_flags = (cmd & CC_mem) ? 0 : Resource::F_disabled;

  io_flags |= Resource::Io_res
           | Resource::F_size_aligned
           | Resource::F_hierarchical
           | Resource::F_can_move;

  mem_flags |= Resource::Mmio_res
            | Resource::F_size_aligned
            | Resource::F_hierarchical
            | Resource::F_can_move;

  Resource *res = 0;
  if (!(x & 1))
    {
      //printf("%08x: BAR[%d] mmio ... %x\n", adr(), bar, x );
      res = new Resource(mem_flags);
      // set ID to 'BARx', x == bar
      res->set_id(0x00524142 + (((l4_uint32_t)('0' + bar)) << 24));
      if ((x & 0x6) == 0x4)
	res->add_flags(Resource::F_width_64bit);

      if (x & 0x8)
	res->add_flags(Resource::F_prefetchable);

      l4_uint64_t size = x & ~0x7f;
      l4_uint64_t a = v & ~0x7f;
      if (res->is_64bit())
	{
	  ++bar;
	  r = 0x10 + bar * 4;
	  cmd = disable_decoders();
	  cfg_read(r, &v, Cfg_long);
	  cfg_write(r, ~0U, Cfg_long);
	  cfg_read(r, &x, Cfg_long);
	  cfg_write(r, v, Cfg_long);
	  restore_decoders(cmd);
	  a |= l4_uint64_t(v) << 32;
	  size |= l4_uint64_t(x) << 32;
	}

      for (int s = 7; s < 64; ++s)
	if ((size >> s) & 1)
	  {
	    size = 1 << s;
	    break;
	  }

      res->start_size(a, size);

      // printf("%08x: BAR[%d] mem ...\n", adr(), bar*4 + 10 );
      _bars[bar - res->is_64bit()] = res;
      if (res->is_64bit())
	_bars[bar] = (Resource*)1;

    }
  else
    {
      // printf("%08x: BAR[%d] io ...\n", adr(), bar );
      int s;
      for (s = 2; s < 32; ++s)
	if ((x >> s) & 1)
	  break;

      res = new Resource(io_flags);
      // set ID to 'BARx', x == bar
      res->set_id(0x00524142 + (((l4_uint32_t)('0' + bar)) << 24));

      _bars[bar] = res;
      res->start_size(v & ~3, 1 << s);
    }

  res->validate();
  _host->add_resource_rq(res);
  return bar + 1;
}

void
Dev::discover_expansion_rom()
{

  if (!Io_config::cfg->expansion_rom(host()))
    return;

  l4_uint32_t v, x;
  unsigned rom_register = ((hdr_type & 0x7f) == 0) ? 12 * 4 : 14 * 4;

  cfg_read(rom_register, &v, Cfg_long);

  if (v == 0xffffffff)
    return; // no expansion ROM

  l4_uint16_t cmd = disable_decoders();
  cfg_write(rom_register, ~0x7ffU, Cfg_long);
  cfg_read(rom_register, &x, Cfg_long);
  cfg_write(rom_register, v, Cfg_long);
  restore_decoders(cmd);

  v &= ~0x3ff;

  if (!x)
    return; // no expansion ROM

  x &= ~0x3ff;

  if (0) printf("ROM %08x: %08x %08x\n", _host->adr(), x, v);

  int s;
  for (s = 2; s < 32; ++s)
    if ((x >> s) & 1)
      break;

  unsigned flags = Resource::Mmio_res | Resource::F_size_aligned
                   | Resource::F_rom | Resource::F_prefetchable
                   | Resource::F_can_move;
  Resource *res = new Resource(flags);
  res->set_id("ROM");

  _rom = res;
  res->start_size(v & ~3, 1 << s);
  res->validate();
  _host->add_resource_rq(res);
}

void
Dev::discover_pci_caps()
{
    {
      l4_uint32_t status;
      cfg_read(Config::Status, &status, Cfg_short);
      if (!(status & CS_cap_list))
	return;
    }

  l4_uint32_t cap_ptr;
  cfg_read(Config::Capability_ptr, &cap_ptr, Cfg_byte);
  cap_ptr &= ~0x3;
  for (; cap_ptr; cfg_read(cap_ptr + 1, &cap_ptr, Cfg_byte), cap_ptr &= ~0x3)
    {
      l4_uint32_t id;
      cfg_read(cap_ptr, &id, Cfg_byte);
      if (0)
        printf("  PCI-cap: ptr: %x -> %x %s %s\n", cap_ptr, id,
               Io_config::cfg->transparent_msi(host()) ? "yes" : "no",
               system_icu()->info.supports_msi() ? "yes" : "no" );
      switch (id)
        {
        case Hw::Pci::Cap::Pm:
          _pm_cap = cap_ptr;
          break;

        case Hw::Pci::Cap::Msi:
          parse_msi_cap(cfg_addr(cap_ptr));
          break;
        case Hw::Pci::Cap::Pcie:
          {
            l4_uint32_t v = cfg_read<l4_uint32_t>(cap_ptr + 4);
            _phantomfn_bits = (v >> 3) & 3;
            break;
          }
        default:
          break;
        }
    }
}

void
Dev::discover_resources(Hw::Device *host)
{

  // printf("survey ... %x.%x\n", dynamic_cast<Hw::Pci::Bus*>(parent())->num, adr());
  l4_uint32_t v;
  cfg_read(Config::Subsys_vendor, &v, Cfg_long);
  subsys_ids = v;
  cfg_read(Config::Irq_pin, &v, Cfg_byte);
  irq_pin = v;

  if (irq_pin)
    {
      Resource * r = new Resource(Resource::Irq_res | Resource::F_relative
                                  | Resource::F_hierarchical,
                                  irq_pin - 1, irq_pin - 1);
      r->set_id("PIN");
      r->dump(0);
      host->add_resource_rq(r);
      //new Resource(Resource::Irq_res | Resource::F_relative
//                                  | Resource::F_hierarchical,
//                                  irq_pin - 1, irq_pin - 1));
    }

  cfg_read(Config::Command, &v, Cfg_short);

  int bars = ((hdr_type & 0x7f) == 0) ? 6 : 2;

  for (int bar = 0; bar < bars;)
    bar = discover_bar(bar);

  discover_expansion_rom();
  discover_pci_caps();

  Dma_domain *d = host->dma_domain();
  if (!d)
    d = host->parent()->dma_domain_for(host);

  Driver *drv = Driver::find(this);
  if (drv)
    drv->probe(this);
}

void
Dev::setup(Hw::Device *)
{
  unsigned decoders_to_enable = 0;
  for (unsigned i = 0; i < sizeof(_bars)/sizeof(_bars[0]); ++i)
    {
      Resource *r = bar(i);
      if (!r)
	continue;

      if (r->empty())
	continue;

      int reg = 0x10 + i * 4;
      l4_uint64_t s = r->start();
      l4_uint16_t cmd = disable_decoders();
      cfg_write(reg, s, Cfg_long);
      if (r->is_64bit())
	{
	  cfg_write(reg + 4, s >> 32, Cfg_long);
	  ++i;
	}
      restore_decoders(cmd);


      l4_uint32_t v;
      cfg_read(reg, &v, Cfg_long);
      l4_uint32_t mask = (r->type() == Resource::Io_res) ? ~0x3 : ~0xf;
      if (l4_uint32_t(v & mask) == l4_uint32_t(s & 0xffffffff))
        decoders_to_enable |= (r->type() == Resource::Io_res) ? 1 : 2;
      else
        {
          decoders_to_enable &= ~((r->type() == Resource::Io_res) ? 1 : 2);
          d_printf(DBG_ERR, "ERROR: could not set PCI BAR %d\n", i);
        }

      if (0)
        printf("%02x:%02x.%x: set BAR[%d] to %08x\n",
               bus_nr(), device_nr(), function_nr(), i, v);
    }

  if (decoders_to_enable)
    {
      l4_uint16_t v = 0;
      cfg_read(Config::Command, &v);
      if ((v & decoders_to_enable) != decoders_to_enable)
        {
          v = (v & ~3) | decoders_to_enable;
          cfg_write(Config::Command, v);
        }
    }
}

void
Dev::pm_save_state(Hw::Device *)
{
  _saved_state.save(Config(cfg_addr(0), bus()));
  flags.state_saved() = true;
}

void
Dev::pm_restore_state(Hw::Device *)
{
  if (flags.state_saved())
    {
      _saved_state.restore(Config(cfg_addr(0), bus()));
      flags.state_saved() = false;
    }
}

bool
Dev::match_cid(cxx::String const &_cid) const
{
  cxx::String const prefix("PCI/");
  cxx::String cid(_cid);
  if (!cid.starts_with(prefix))
    return false;

  cid = cid.substr(prefix.len());
  cxx::String::Index n;
  for (; cid.len() > 0; cid = cid.substr(n + 1))
    {
      n = cid.find("&");
      cxx::String tok = cid.head(n);
      if (tok.empty())
	continue;

      if (tok.starts_with("CC_"))
	{
	  tok = tok.substr(3);
	  if (tok.len() < 2)
	    return false;

	  l4_uint32_t _csr;
	  int l = tok.from_hex(&_csr);
	  if (l < 0 || l > 6 || l % 2)
	    return false;

	  if ((cls_rev >> (8 + (6-l) * 4)) == _csr)
	    continue;
	  else
	    return false;
	}
      else if (tok.starts_with("REV_"))
	{
	  tok = tok.substr(4);
	  unsigned char r;
	  if (tok.len() != 2 || tok.from_hex(&r) != 2)
	    return false;

	  if (r != (cls_rev & 0xff))
	    return false;
	}
      else if (tok.starts_with("VEN_"))
	{
	  tok = tok.substr(4);
	  l4_uint32_t v;
	  if (tok.len() != 4 || tok.from_hex(&v) != 4)
	    return false;

	  if ((vendor_device & 0xffff) != v)
	    return false;
	}
      else if (tok.starts_with("DEV_"))
	{
	  tok = tok.substr(4);
	  l4_uint32_t d;
	  if (tok.len() != 4 || tok.from_hex(&d) != 4)
	    return false;

	  if (((vendor_device >> 16) & 0xffff) != d)
	    return false;
	}
      else if (tok.starts_with("SUBSYS_"))
	{
	  l4_uint32_t s;
	  tok = tok.substr(7);
	  if (tok.len() != 8 || tok.from_hex(&s) != 8)
	    return false;
	  if (subsys_ids != s)
	    return false;
	}
      else if (tok.starts_with("ADR_"))
        {
          cxx::String adr = tok.substr(4);
          l4_uint32_t segment, bus, device, function;

          for (;;)
            {
              unsigned l;

              l = adr.from_hex(&segment);
              if (l == 0)
                break;

              adr = adr.substr(l);
              if (*adr.start() != ':')
                break;

              adr = adr.substr(1);
              l = adr.from_hex(&bus);
              if (l == 0)
                break;

              adr = adr.substr(l);
              if (*adr.start() != ':')
                break;

              adr = adr.substr(1);
              l = adr.from_hex(&device);
              if (l == 0)
                break;

              adr = adr.substr(l);
              if (*adr.start() != '.')
                break;

              adr = adr.substr(1);
              l = adr.from_hex(&function);
              if (l == 0)
                break;

              return    segment == 0          && bus_nr() == bus
                     && device_nr() == device && function_nr() == function;
            }

          d_printf(DBG_ERR, "error: PCI/ADR_xxxx:xx:xx.x format error: %.*s\n",
                   tok.len(), tok.start());
          return false;
        }
      else
	return false;
    }

  return true;
}

Pci_pci_bridge_basic::Pci_pci_bridge_basic(Hw::Device *host, Bus *bus,
                                           l4_uint8_t hdr_type)
: Bus(0, Pci_bus), Dev(host, bus, hdr_type), pri(0)
{
  if (bus->bus_type == Pci_express_bus)
    {
      Cap pcie = find_pci_cap(Cap::Pcie);
      if (pcie.is_valid())
        {
          l4_uint16_t r2; pcie.read(2, &r2);
          switch ((r2 >> 4) & 0xf) // device/type
            {
            case 0x4: // Root Port of PCI Express Root Complex
            case 0x5: // Upstream Port of PCI Express Switch
            case 0x6: // Downstream Port of PCI Express Switch
              bus_type = Bus::Pci_express_bus;
              break;
            default:
              // all other ids are either no busses or
              // legacy PCI/PCI-X busses
              break;
            }
        }
    }
}


void
Pci_pci_bridge::setup_children(Hw::Device *)
{
  if (!mmio->empty() && mmio->valid())
    {
      l4_uint32_t v = (mmio->start() >> 16) & 0xfff0;
      v |= mmio->end() & 0xfff00000;
      cfg_write(0x20, v, Cfg_long);
      // printf("%08x: set mmio to %08x\n", adr(), v);
      l4_uint32_t r;
      cfg_read(0x20, &r, Cfg_long);
      // printf("%08x: mmio =      %08x\n", adr(), r);
      cfg_read(0x04, &r, Cfg_short);
      r |= 3;
      cfg_write(0x4, r, Cfg_short);
    }

  if (!pref_mmio->empty() && pref_mmio->valid())
    {
      l4_uint32_t v = (pref_mmio->start() >> 16) & 0xfff0;
      v |= pref_mmio->end() & 0xfff00000;
      cfg_write(0x24, v, Cfg_long);
      // printf("%08x: set pref mmio to %08x\n", adr(), v);
    }
}

void
Pci_pci_bridge::discover_resources(Hw::Device *host)
{

  l4_uint32_t v;
  l4_uint64_t s, e;

  cfg_read(Config::Mem_base, &v, Cfg_long);
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

  // printf("%08x: mmio = %08x\n", adr(), v);
  mmio = r;
  mmio->validate();
  _host->add_resource_rq(mmio);

  r = new Resource_provider(Resource::Mmio_res | Resource::F_prefetchable
                            | Resource::F_can_move | Resource::F_can_resize);
  r->set_id("WIN1");
  cfg_read(Config::Pref_mem_base, &v, Cfg_long);
  s = (v & 0xfff0) << 16;
  e = (v & 0xfff00000) | 0xfffff;

  if ((v & 0x0f) == 1)
    {
      r->add_flags(Resource::F_width_64bit);
      cfg_read(Config::Pref_mem_base_hi, &v, Cfg_long);
      s |= l4_uint64_t(v) << 32;
      cfg_read(Config::Pref_mem_limit_hi, &v, Cfg_long);
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

  cfg_read(Config::Io_base, &v, Cfg_short);
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

  if (bus_type != Pci_express_bus)
    {
      if (bus()->bus_type != Pci_express_bus)
        // use the DMA domain of the bridge device itself as downstream DMA
        // domain
        host->set_downstream_dma_domain(host->parent()->dma_domain_for(host));
      else
        host->set_downstream_dma_domain(host->dma_domain_for(0));
    }

  Dev::discover_resources(host);
}


void
Root_bridge::setup(Hw::Device *host)
{
  for (Resource_list::const_iterator i = host->resources()->begin();
       i != host->resources()->end(); ++i)
    if ((*i)->type() == Resource::Bus_res)
      num = subordinate = (*i)->start();

  Bus::discover_bus(host);
  Bus::setup(host);
}

static const char * const pci_classes[] =
    { "unknown", "mass storage contoller", "network controller", 
      "display controller", "multimedia device", "memory controller",
      "bridge device", "simple communication controller", 
      "system peripheral", "input device", "docking station", 
      "processor", "serial bus controller", "wireless controller",
      "intelligent I/O controller", "satellite communication controller",
      "encryption/decryption controller", 
      "data aquisition/signal processing controller" };

static char const * const pci_bridges[] =
{ "Host/PCI Bridge", "ISA Bridge", "EISA Bridge", "Micro Channel Bridge",
  "PCI Bridge", "PCMCIA Bridge", "NuBus Bridge", "CardBus Bridge" };


#if 0
static void
dump_res_rec(Resource_list const *r, int indent)
{
  for (Resource_list::iterator i = r->begin(); i!= r->end(); ++i)
    if (*i)
      {
        i->dump(indent + 2);
        //dump_res_rec(i->child(), indent + 2);
      }
}
#endif

void
Dev::dump(int indent) const
{
  char const *classname = "";

  if (cls_rev >> 24 < sizeof(pci_classes)/sizeof(pci_classes[0]))
    classname = pci_classes[cls_rev >> 24];

  if ((cls_rev >> 24) == 0x06)
    {
      unsigned sc = (cls_rev >> 16) & 0xff;
      if (sc < sizeof(pci_bridges)/sizeof(pci_bridges[0]))
	classname = pci_bridges[sc];
    }

  printf("%*.s%04x:%02x:%02x.%x: %s [%d]\n", indent, " ",
         0, (int)_bus->num, _host->adr() >> 16, _host->adr() & 0xffff,
         classname, (int)hdr_type);

  printf("%*.s              0x%04x 0x%04x\n", indent, " ", vendor(), device());
#ifdef CONFIG_L4IO_PCIID_DB
  char buf[130];
  libpciids_name_device(buf, sizeof(buf), vendor(), device());
  printf("%*.s              %s\n", indent, " ", buf);
#endif
#if 0
  if (verbose_lvl)
    dump_res_rec(resources(), 0);
#endif
}

void
Bus::dump(int) const
{
#if 0
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



void
Pci_cardbus_bridge::discover_resources(Hw::Device *host)
{
  l4_uint32_t v;
  cfg_read(Config::Subsys_vendor, &v, Cfg_long);
  subsys_ids = v;

  Resource *r = new Resource_provider(Resource::Mmio_res | Resource::F_can_move
                                      | Resource::F_can_resize);
  r->set_id("WIN0");
  cfg_read(Config::Cb_mem_base_0, &v, Cfg_long);
  r->start(v);
  cfg_read(Config::Cb_mem_limit_0, &v, Cfg_long);
  r->end(v);
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Mmio_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN1");
  cfg_read(Config::Cb_mem_base_1, &v, Cfg_long);
  r->start(v);
  cfg_read(Config::Cb_mem_limit_1, &v, Cfg_long);
  r->end(v);
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN2");
  cfg_read(Config::Cb_io_base_0, &v, Cfg_long);
  r->start(v);
  cfg_read(Config::Cb_io_limit_0, &v, Cfg_long);
  r->end(v);
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);

  r = new Resource_provider(Resource::Io_res | Resource::F_can_move
                            | Resource::F_can_resize);
  r->set_id("WIN3");
  cfg_read(Config::Cb_io_base_1, &v, Cfg_long);
  r->start(v);
  cfg_read(Config::Cb_io_limit_1, &v, Cfg_long);
  r->end(v);
  if (!r->end())
    r->set_empty();
  r->validate();
  host->add_resource_rq(r);
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


Saved_cap *
Saved_config::find_cap(l4_uint8_t type)
{
  for (auto c = _caps.begin(); c != _caps.end(); ++c)
    if (c->type() == type)
      return *c;

  return 0;
}

void
Saved_config::save(Config cfg)
{
  for (unsigned i = 0; i < 16; ++i)
    cfg.read(i * 4, &_regs.w[i]);

  for (auto c = _caps.begin(); c != _caps.end(); ++c)
    c->save(cfg);
}

static void
restore_cfg_word(Config cfg, l4_uint32_t value, int retry)
{
  l4_uint32_t v;
  cfg.read(0, &v);
  if (v == value)
    return;

  for (;;)
    {
      cfg.write(0, value);
      if (retry-- <= 0)
        return;

      cfg.read(0, &v);
      if (v == value)
        return;

      l4_sleep(1);
    }
}

static void
restore_cfg_range(Config cfg, l4_uint32_t *saved,
                  unsigned start, unsigned end, unsigned retry = 0)
{
  for (unsigned i = start; i <= end; ++i)
    restore_cfg_word(cfg + (i * 4), saved[i], retry);
}

void
Saved_config::restore(Config cfg)
{
  Saved_cap *pcie = find_cap(Cap::Pcie);

  // PCI express state must be restored first
  if (pcie)
    pcie->restore(cfg);

  if ((read<l4_uint8_t>(Config::Header_type) & 0x7f) == 0)
    {
      restore_cfg_range(cfg, _regs.w, 10, 15);
      restore_cfg_range(cfg, _regs.w, 4, 9, 10); //< BARs
      restore_cfg_range(cfg, _regs.w, 0, 3);     //< command and status etc.
    }
  else
    // we do a dumb restore for bridges
    restore_cfg_range(cfg, _regs.w, 0, 15);

  for (auto c = _caps.begin(); c != _caps.end(); ++c)
    if (c->type() != Cap::Pcie) // skip the PCI express state (already restored)
      c->restore(cfg);
}

bool
Driver::register_driver_for_class(l4_uint32_t device_class)
{
  driver_for_class()[device_class] = this;
  return true;
}

bool
Driver::register_driver(l4_uint16_t vendor, l4_uint16_t device)
{
  driver_for_vendor_device()[((l4_uint32_t)device) << 16 | (l4_uint32_t)vendor] = this;
  return true;
}

Driver *
Driver::find(Dev *dev)
{
  //d_printf(DBG_DEBUG, "find(device_class = %x, vendor = %x, device = %x\n", device_class, vendor, device);

  Drv_list const &cls = driver_for_class();
  Drv_list const &vd = driver_for_vendor_device();

  // first take vendor and device IDs
  Drv_list::const_iterator r = vd.find(dev->vendor_device_ids());
  if (r != vd.end())
    return (*r).second;

  // use class IDs
  r = cls.find(dev->class_rev() >> 16);
  if (r != cls.end())
    return (*r).second;

  return 0;
}

}}

