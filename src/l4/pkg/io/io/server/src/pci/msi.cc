/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "pci.h"

#include "cfg.h"
#include "debug.h"
#include "hw_msi.h"
#include "main.h"

namespace Hw { namespace Pci {

namespace {

class Saved_msi_cap : public Saved_cap
{
public:
  explicit Saved_msi_cap(unsigned pos) : Saved_cap(Cap::Msi, pos) {}

private:
  enum
  {
    Control = 0x02,
    Addr_lo = 0x04,
    Addr_hi = 0x08,
    Data_32 = 0x08,
    Data_64 = 0x0c
  };

  l4_uint32_t addr_lo, addr_hi;
  l4_uint16_t data;
  l4_uint16_t control;

  void _save(Config cap)
  {
    cap.read(Control, &control);
    cap.read(Addr_lo, &addr_lo);
    if (control & (1 << 7))
      {
        cap.read(Addr_hi, &addr_hi);
        cap.read(Data_64, &data);
      }
    else
      cap.read(Data_32, &data);
  }

  void _restore(Config cap)
  {
    cap.write(Addr_lo, addr_lo);
    if (control & (1 << 7))
      {
        cap.write(Addr_hi, addr_hi);
        cap.write(Data_64, data);
      }
    else
      cap.write(Data_32, data);
    cap.write(Control, control);
  }

};

static unsigned _last_msi;

class Msi
{
public:
  enum Addrs
  {
    Base_hi = 0,
    Base_lo = 0xfee00000,
  };

  enum Mode
  {
    Dest_mode_phys = 0 << 2,
    Dest_mode_log  = 1 << 2,
  };

  enum Redir
  {
    Dest_redir_cpu     = 0 << 3,
    Dest_redir_lowprio = 1 << 3,
  };

  enum Delivery
  {
    Data_del_fixed   = 0 << 8,
    Data_del_lowprio = 1 << 8,
  };

  enum Level
  {
    Data_level_deassert = 0 << 14,
    Data_level_assert   = 1 << 14,
  };

  enum Trigger
  {
    Data_trigger_edge  = 0 << 15,
    Data_trigger_level = 1 << 15,
  };

  static unsigned data(int vector) { return vector & 0xff; }
};

class Msi_res : public Hw::Msi_resource, public Transparent_msi
{
public:
  Msi_res(unsigned msi, Dev *dev, l4_uint32_t cap_offset)
  : Msi_resource(msi), _dev(dev), _cap(cap_offset)
  {}

  int bind(Triggerable const &irq, unsigned mode) override;
  int unbind(bool deleted) override;
  int msi_info(Msi_src *, l4_icu_msi_info_t *) override
  { return -L4_EINVAL; }

  void dump(int indent) const override
  { Resource::dump("MSI   ", indent);  }

  l4_uint32_t filter_cmd_read(l4_uint32_t cmd) override;
  l4_uint16_t filter_cmd_write(l4_uint16_t cmd, l4_uint16_t ocmd) override;

private:
  void setup_cap(l4_uint16_t ctl, l4_uint16_t cmd);
  Dev *_dev;
  l4_uint32_t _cap;
  l4_icu_msi_info_t _msg;
};

void
Msi_res::setup_cap(l4_uint16_t ctl, l4_uint16_t cmd)
{
  unsigned msg_offs = 8;
  if (ctl & (1 << 7))
    msg_offs = 12;

  // disable INTx if not already
  if (!(cmd & Dev::CC_int_disable))
    _dev->cfg_write<l4_uint16_t>(Config::Command, cmd | Dev::CC_int_disable);

  _dev->cfg_write<l4_uint32_t>(_cap + 4, _msg.msi_addr);

  if (ctl & (1 << 7))
    _dev->cfg_write<l4_uint32_t>(_cap + 8, _msg.msi_addr >> 32);

  _dev->cfg_write<l4_uint16_t>(_cap + msg_offs, _msg.msi_data);

  _dev->cfg_write<l4_uint16_t>(_cap + 2, ctl | 1);

  d_printf(DBG_DEBUG2, "MSI: enable kernel PIN=%x hwpci=%02x:%02x.%x: reg=%03x msg=%llx:%x\n",
           pin(), _dev->bus_nr(), _dev->device_nr(), _dev->function_nr(),
           _cap, _msg.msi_addr, _msg.msi_data);
}

int
Msi_res::bind(Triggerable const &irq, unsigned mode)
{
  int err = Msi_resource::bind(irq, mode);
  if (err < 0)
    return err;

  _msg = l4_icu_msi_info_t();
  // TODO: use a source filter for transparent MSIs too
  int e2 = l4_error(system_icu()->icu->msi_info(pin(), 0, &_msg));
  if (e2 < 0)
    {
      d_printf(DBG_ERR, "ERROR: could not get MSI message (pin=%x)\n", pin());
      Msi_resource::unbind(false);
      return e2;
    }

  // MSI capability
  l4_uint16_t ctl = _dev->cfg_read<l4_uint16_t>(_cap + 2);
  setup_cap(ctl, _dev->cfg_read<l4_uint16_t>(Config::Command));
  return 0;
}

int
Msi_res::unbind(bool deleted)
{
  // disable MSI
  l4_uint16_t ctl;
  ctl = _dev->cfg_read<l4_uint16_t>(_cap + 2);
  _dev->cfg_write<l4_uint16_t>(_cap + 2, ctl & ~1);

  return Msi_resource::unbind(deleted);
}

l4_uint32_t
Msi_res::filter_cmd_read(l4_uint32_t cmd)
{
  if (!this->irq())
    {
      // disable INTx if not already
      if (!(cmd & Dev::CC_int_disable))
        _dev->cfg_write<l4_uint16_t>(Config::Command, cmd | Dev::CC_int_disable);

      // no transparent MSI bound, nothing to check
      return cmd;
    }

  l4_uint16_t ctl = _dev->cfg_read<l4_uint16_t>(_cap + 2);
  if (!(cmd & Dev::CC_int_disable) || !(ctl & 1))
    // MSI was disabled, rewrite the MSI cap
    setup_cap(ctl, cmd);

  return cmd;
}

l4_uint16_t
Msi_res::filter_cmd_write(l4_uint16_t cmd, l4_uint16_t ocmd)
{
  if (!this->irq())
    {
      cmd |= Dev::CC_int_disable;
      return cmd;
    }

  l4_uint16_t ctl = _dev->cfg_read<l4_uint16_t>(_cap + 2);
  if (!(ocmd & Dev::CC_int_disable) || !(ctl & 1))
    // MSI was disabled, rewrite the MSI cap
    setup_cap(ctl, ocmd);

  cmd |= Dev::CC_int_disable;
  return cmd;

}

}


void
Dev::parse_msi_cap(Cfg_addr cap_ptr)
{
  if (   !Io_config::cfg->transparent_msi(host())
      || !system_icu()->info.supports_msi())
    return;

  unsigned msi = _last_msi++;

  for (Resource_list::const_iterator i = host()->resources()->begin();
      i != host()->resources()->end(); ++i)
    {
      if ((*i)->type() == Resource::Irq_res)
        {
          (*i)->set_empty();
          (*i)->add_flags(Resource::F_disabled);
        }
    }

  d_printf(DBG_DEBUG, "Use MSI PCI device %02x:%02x:%x: pin=%x\n",
           bus()->num, host()->adr() >> 16, host()->adr() & 0xff, msi);

  auto *res = new Msi_res(msi, this, cap_ptr.reg());
  flags.msi() = true;
  _host->add_resource_rq(res);
  _transp_msi = res;
  _saved_state.add_cap(new Saved_msi_cap(cap_ptr.reg()));
}

}}
