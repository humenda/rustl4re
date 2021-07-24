/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */

#include <pci-dev.h>

#include "main.h"
#include "cfg.h"
// -----

// for the printf in discover_pci_caps
#include <cstdio>

#ifdef CONFIG_L4IO_PCIID_DB
# include "pciids.h"
#endif

namespace Hw { namespace Pci {

cxx::H_list_t<Extended_cap_handler> Dev::_ext_cap_handlers;

void
Config_cache::_discover_pci_caps(Config const &c)
{
  if (!cap_list)
    return;

  l4_uint8_t cap_ptr = c.read<l4_uint8_t>(cap_list) & ~0x3;
  while (cap_ptr)
    {
      l4_uint16_t cl = c.read<l4_uint16_t>(cap_ptr);
      l4_uint8_t id  = cl & 0xff;

      switch (id)
        {
        case Hw::Pci::Cap::Pm:
          pm_cap = cap_ptr;
          break;

        case Hw::Pci::Cap::Pcie:
          pcie_cap = cap_ptr;
          pcie_type = (c.read<l4_uint16_t>(pcie_cap + 2) >> 4) & 0xf;
          break;

        default:
          break;
        }

      cap_ptr = (cl >> 8) & 0xfc;
    }
}


void
Config_cache::fill(l4_uint32_t _vendor_device, Config const &c)
{
  *static_cast<Config *>(this) = c;
  vendor_device = _vendor_device;

  cls_rev    = c.read<l4_uint32_t>(Config::Class_rev);
  hdr_type   = c.read<l4_uint8_t>(Config::Header_type);

  switch (type())
    {
    case 0:
      subsys_ids = c.read<l4_uint32_t>(Config::Subsys_vendor);
      cap_list = 0x34;
      break;

    case 1: // PCI-PCI bridge
      cap_list = 0x34;
      break;

    case 2: // PCI-CardBus bridge
      subsys_ids = c.read<l4_uint32_t>(0x40);
      cap_list = 0x14;
      break;
    }

  l4_uint16_t status = c.read<l4_uint16_t>(Config::Status);

  if (!(status & 0x10))
    cap_list = 0; // no PCI caps if this bit is zero...

  irq_pin    = c.read<l4_uint8_t>(Config::Irq_pin);
  _discover_pci_caps(c);
}


l4_uint32_t
Dev::vendor_device_ids() const
{ return cfg.vendor_device; }

l4_uint32_t
Dev::class_rev() const
{ return cfg.cls_rev; }

l4_uint32_t
Dev::subsys_vendor_ids() const
{ return cfg.subsys_ids; }

unsigned
Dev::bus_nr() const
{ return cfg.addr().bus(); }

l4_uint32_t
Dev::checked_cmd_read()
{
  l4_uint32_t cs = config().read<l4_uint32_t>(Config::Command);
  if (_transp_msi)
    return _transp_msi->filter_cmd_read(cs);

  return cs;
}

l4_uint16_t
Dev::checked_cmd_write(l4_uint16_t mask, l4_uint16_t cmd)
{
  l4_uint16_t ocmd = config().read<l4_uint16_t>(Config::Command);
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

  config().write<l4_uint16_t>(Pci::Config::Command, ncmd);
  return ncmd;
}

l4_uint32_t
Dev::recheck_bars(unsigned enable_decoders)
{
  if (!enable_decoders)
    return 0;

  auto c = config();

  unsigned next_bar;
  for (unsigned i = 0; i < 6; i += next_bar)
    {
      l4_uint32_t const bar_addr = Pci::Config::Bar_0 + i * 4;
      l4_uint32_t bar = c.read<l4_uint32_t>(bar_addr);
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
          addr = c.read<l4_uint32_t>(bar_addr + 4);
          addr <<= 32;
        }

      addr |= bar & mask;

      if (r->start() == addr)
        continue; // everything is fine

      addr = r->start();
      if (is_64bit)
        {
          c.write<l4_uint32_t>(bar_addr + 4, addr >> 32);
          if (c.read<l4_uint32_t>(bar_addr + 4) != addr >> 32)
            {
              d_printf(DBG_ERR, "error: PCI BAR refused write: bar=%d\n", i);
              enable_decoders &= ~decoder;
              continue;
            }
        }

      bar = (bar & ~mask) | (addr & mask);

      c.write(bar_addr, bar);
      if (c.read<l4_uint32_t>(bar_addr) != bar)
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

  auto c = config();

  l4_uint32_t r = c.read<l4_uint32_t>(Config::Rom_address);

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
  c.write(Config::Rom_address, r);
  return c.read<l4_uint32_t>(Config::Rom_address) == r;
}

unsigned
Dev::disable_decoders()
{
  auto c = config();
  // disable decoders, and mask IRQs
  l4_uint16_t v = c.read<l4_uint16_t>(Config::Command);
  c.write<l4_uint16_t>(Config::Command, (v & ~3) | (1<<10));
  return v & 0xff;
}

void
Dev::restore_decoders(l4_uint16_t cmd)
{
  config().write(Config::Command, cmd);
}

Cap
Dev::find_pci_cap(unsigned char id)
{
  l4_uint32_t cap_ptr;

  switch (cfg.type())
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
                         __func__, cfg.type());
      return Cap();
    }

  l4_uint8_t o = config().read<l4_uint8_t>(cap_ptr);
  if (o == 0)
    return Cap();

  for (Cap c = config(o); c.is_valid(); c = c.next())
    if (c.id() == id)
      return c;

  return Cap();
}

Extended_cap
Dev::find_ext_cap(unsigned id)
{
  if (!is_pcie())
    return Extended_cap();

  l4_uint16_t offset = 0x100;
  for (;;)
    {
      Hw::Pci::Extended_cap cap = config(offset);

      if (!cap.is_valid())
        return Extended_cap();

      if (cap.id() == id)
        return cap;

      offset = cap.next();
      if (!offset)
        return Extended_cap();
    }
}

int
Dev::discover_bar(int bar)
{
  Cfg_bar c = config(Config::Bar_0 + bar * 4);

  _bars[bar] = 0;
  l4_uint16_t cmd = disable_decoders();
  bool valid = c.parse();
  restore_decoders(cmd);

  if (!valid) // skip invalid (empty) BAR
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
  switch (c.type())
    {
    case Cfg_bar::T_mmio:
      res = new Resource(mem_flags);
      // set ID to 'BARx', x == bar
      res->set_id(0x00524142 + (((l4_uint32_t)('0' + bar)) << 24));
      _bars[bar] = res;

      if (c.is_64bit())
        {
          res->add_flags(Resource::F_width_64bit);
          _bars[++bar] = nullptr;
        }

      if (c.is_prefetchable())
        res->add_flags(Resource::F_prefetchable);

      res->start_size(c.base(), c.size());
      break;
    case Cfg_bar::T_io:
      res = new Resource(io_flags);
      // set ID to 'BARx', x == bar
      res->set_id(0x00524142 + (((l4_uint32_t)('0' + bar)) << 24));

      _bars[bar] = res;
      res->start_size(c.base(), c.size());
    }

  res->validate();
  _host->add_resource_rq(res);
  return bar + 1;
}

void
Dev::discover_expansion_rom()
{
  l4_uint32_t v, x;
  unsigned rom_register = (cfg.type() == 0) ? 12 * 4 : 14 * 4;

  auto c = config();

  v = c.read<l4_uint32_t>(rom_register);

  if (v == 0xffffffff)
    return; // no expansion ROM

  l4_uint16_t cmd = disable_decoders();
  c.write<l4_uint32_t>(rom_register, ~0x7ffU);
  x = c.read<l4_uint32_t>(rom_register);
  // write value back and disable expansion ROM
  c.write<l4_uint32_t>(rom_register, v & ~0x1);
  restore_decoders(cmd);

  v &= ~0x7ff;

  if (!x)
    return; // no expansion ROM

  x &= ~0x7ff;

  if (0)
    printf("ROM %08x: %08x %08x\n", _host->adr(), x, v);

  int s;
  for (s = 2; s < 32; ++s)
    if ((x >> s) & 1)
      break;

  unsigned flags = Resource::Mmio_res | Resource::F_size_aligned
                   | Resource::F_rom | Resource::F_prefetchable
                   | Resource::F_can_move;
  Resource *res = new Resource(flags);
  res->set_id("ROM");

  if (Io_config::cfg->expansion_rom(host()))
    _rom = res;

  res->start_size(v & ~3, 1 << s);
  res->validate();
  _host->add_resource_rq(res);
}

void
Dev::discover_pcie_caps()
{
  l4_uint16_t offset = 0x100;

  for (;;)
    {
      Hw::Pci::Extended_cap cap = config(offset);

      if (offset == 0x100 && !cap.is_valid())
        return;

      l4_uint32_t hdr = cap.header();

      for (auto h: _ext_cap_handlers)
        {
          if (h->match(hdr & 0xfffff))
            h->handle_cap(this, cap);
        }

      offset = cap.next();
      if (!offset)
        return;
    }
}

void
Dev::discover_pci_caps()
{
  auto c = config();
    {
      l4_uint16_t status = c.read<l4_uint16_t>(Config::Status);
      if (!(status & CS_cap_list))
        return;
    }

  l4_uint32_t cap_ptr = c.read<l4_uint8_t>(Config::Capability_ptr);
  cap_ptr &= ~0x3;
  for (; cap_ptr; cap_ptr = c.read<l4_uint8_t>(cap_ptr + 1) & ~0x3)
    {
      l4_uint32_t id = c.read<l4_uint8_t>(cap_ptr);
      if (0)
        printf("  PCI-cap: ptr: %x -> %x %s %s\n", cap_ptr, id,
               Io_config::cfg->transparent_msi(host()) ? "yes" : "no",
               system_icu()->info.supports_msi() ? "yes" : "no" );
      switch (id)
        {
        case Hw::Pci::Cap::Msi:
          parse_msi_cap(cfg_addr(cap_ptr));
          break;
        case Hw::Pci::Cap::Pcie:
          {
            l4_uint32_t v = c.read<l4_uint32_t>(cap_ptr + 4);
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
  if (flags.discovered())
    return;

  if (0)
    printf("survey ... %x.%x\n", cfg.addr().bus(), host->adr());

  if (cfg.irq_pin)
    {
      Resource * r = new Resource(Resource::Irq_res | Resource::F_relative
                                  | Resource::F_hierarchical,
                                  cfg.irq_pin - 1, cfg.irq_pin - 1);
      r->set_id("PIN");
      host->add_resource_rq(r);
    }

  int bars = cfg.nbars();

  for (int bar = 0; bar < bars;)
    bar = discover_bar(bar);

  discover_expansion_rom();
  discover_pci_caps();

  Cap pcie = find_pci_cap(Cap::Pcie);
  if (pcie.is_valid())
      discover_pcie_caps();

  Dma_domain *d = host->dma_domain();
  if (!d)
    d = host->parent()->dma_domain_for(host);

  flags.discovered() = true;
}

void
Dev::setup(Hw::Device *)
{
  auto c = config();
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
      c.write<l4_uint32_t>(reg, s);
      if (r->is_64bit())
        {
          c.write<l4_uint32_t>(reg + 4, s >> 32);
          ++i;
        }
      restore_decoders(cmd);


      l4_uint32_t v = c.read<l4_uint32_t>(reg);
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
      l4_uint16_t v = c.read<l4_uint16_t>(Config::Command);
      if ((v & decoders_to_enable) != decoders_to_enable)
        {
          v = (v & ~3) | decoders_to_enable;
          c.write<l4_uint16_t>(Config::Command, v);
        }
    }
}

void
Dev::pm_save_state(Hw::Device *)
{
  _saved_state.save(this);
  flags.state_saved() = true;
}

void
Dev::pm_restore_state(Hw::Device *)
{
  if (flags.state_saved())
    {
      _saved_state.restore(this);
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

          if ((cfg.cls_rev >> (8 + (6-l) * 4)) == _csr)
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

          if (r != (cfg.cls_rev & 0xff))
            return false;
        }
      else if (tok.starts_with("VEN_"))
        {
          tok = tok.substr(4);
          l4_uint32_t v;
          if (tok.len() != 4 || tok.from_hex(&v) != 4)
            return false;

          if ((cfg.vendor_device & 0xffff) != v)
            return false;
        }
      else if (tok.starts_with("DEV_"))
        {
          tok = tok.substr(4);
          l4_uint32_t d;
          if (tok.len() != 4 || tok.from_hex(&d) != 4)
            return false;

          if (((cfg.vendor_device >> 16) & 0xffff) != d)
            return false;
        }
      else if (tok.starts_with("SUBSYS_"))
        {
          l4_uint32_t s;
          tok = tok.substr(7);
          if (tok.len() != 8 || tok.from_hex(&s) != 8)
            return false;
          if (cfg.subsys_ids != s)
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

static const char * const pci_classes[] =
{
  /* 0x00 */ "legacy",
  /* 0x01 */ "mass storage controller",
  /* 0x02 */ "network controller",
  /* 0x03 */ "display controller",
  /* 0x04 */ "multimedia device",
  /* 0x05 */ "memory controller",
  /* 0x06 */ "bridge device",
  /* 0x07 */ "simple communication controller",
  /* 0x08 */ "system peripheral",
  /* 0x09 */ "input device",
  /* 0x0a */ "docking station",
  /* 0x0b */ "processor",
  /* 0x0c */ "serial bus controller",
  /* 0x0d */ "wireless controller",
  /* 0x0e */ "intelligent I/O controller",
  /* 0x0f */ "satellite communication controller",
  /* 0x10 */ "encryption/decryption controller",
  /* 0x11 */ "data acquisition/signal processing controller",
  /* 0x12 */ "processing accelerator",
  /* 0x13 */ "non-essential instrumentation function"
};

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

  if (cfg.cls_rev >> 24 < sizeof(pci_classes)/sizeof(pci_classes[0]))
    classname = pci_classes[cfg.cls_rev >> 24];

  if ((cfg.cls_rev >> 24) == 0x06)
    {
      unsigned sc = (cfg.cls_rev >> 16) & 0xff;
      if (sc < sizeof(pci_bridges)/sizeof(pci_bridges[0]))
        classname = pci_bridges[sc];
    }

  printf("%*.s%04x:%02x:%02x.%x: %s (0x%06x) [%d]\n", indent, " ",
         0, (int)bus_nr(), cfg.addr().dev(), cfg.addr().fn(),
         classname, cfg.cls_rev >> 8, (int)cfg.hdr_type);

  printf("%*.s0x%04x 0x%04x\n", indent + 14, " ", cfg.vendor(), cfg.device());
#ifdef CONFIG_L4IO_PCIID_DB
  char buf[130];
  libpciids_name_device(buf, sizeof(buf), cfg.vendor(), cfg.device());
  printf("%*.s%s\n", indent + 14, " ", buf);
#endif
#if 0
  if (verbose_lvl)
    dump_res_rec(resources(), 0);
#endif
}

} }
