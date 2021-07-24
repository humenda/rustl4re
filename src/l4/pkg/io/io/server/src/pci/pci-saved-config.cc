/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */

#include <pci-saved-config.h>

// there is some stray l4_sleep in the code used during config space restore
#include <l4/util/util.h>

namespace Hw { namespace Pci {

Saved_cap *
Saved_config::find_cap(l4_uint8_t type)
{
  for (auto c = _caps.begin(); c != _caps.end(); ++c)
    if (c->type() == type)
      return *c;

  return 0;
}

void
Saved_config::save(If *dev)
{
  auto cfg = dev->config();
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

      // MUAHHHH:
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
Saved_config::restore(If *dev)
{
  Saved_cap *pcie = find_cap(Cap::Pcie);

  auto cfg = dev->config();

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

}}
