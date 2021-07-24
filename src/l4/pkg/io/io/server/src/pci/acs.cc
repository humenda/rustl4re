// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019-2020 Kernkonzept GmbH.
 * Author(s): Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#include <pci-dev.h>
#include <pci-caps.h>

#include "cfg.h"
#include "debug.h"
#include "hw_msi.h"
#include "main.h"

namespace Hw { namespace Pci {

namespace {

class Saved_acs_cap : public Saved_cap
{
public:
  explicit Saved_acs_cap(unsigned offset) : Saved_cap(Extended_cap::Acs, offset) {}

private:
  enum Regs
  {
    Capabilities = 0x4,
    Control      = 0x6,
  };

  l4_uint16_t control;

  void _save(Config cap) override
  {
    cap.read(Control, &control);
  }

  void _restore(Config cap) override
  {
    cap.write(Control, control);
  }
};

struct Acs_cap_handler : Extended_cap_handler_t<Acs_cap::Id>
{
  bool handle_cap(Dev *dev, Extended_cap acs_cap) const override
  {
    if (!acs_cap.is_valid())
      return false;

    auto caps = acs_cap.read<Acs_cap::Caps>();
    auto ctrl = acs_cap.read<Acs_cap::Ctrl>();

    // always disable egress control and direct translated P2P
    ctrl.p2p_egress_ctrl_enable() = 0;
    ctrl.direct_translated_p2p_enable() = 0;

    // enable all other supported ACS features
    ctrl.f() = caps.f();

    acs_cap.write(ctrl);

    // Certain Intel PCIe root ports implement ACS but instead of using words
    // for the capability and control registers they use dwords. This means the
    // control register is at offset 8 instead of 6. So we hopefully tried to
    // write into an RO area.
    // We read the control word and compare it with our desired configuration.
    // If they don't match we print a warning.
    auto c = acs_cap.read<Acs_cap::Ctrl>();

    if (c.enabled() != ctrl.enabled())
      {
        d_printf(DBG_ERR,
                 "Error: PCI ACS control does not match desired configuration. "
                 "Is this a buggy PCIe root port?\n");
        return false;
      }

    d_printf(DBG_DEBUG, "ACS: %02x:%02x.%x: enabled ACS (features=0x%x)\n",
             dev->bus_nr(), dev->device_nr(), dev->function_nr(),
             ctrl.enabled().get());

    dev->add_saved_cap(new Saved_acs_cap(acs_cap.reg()));

    return true;
  }

};

static Acs_cap_handler _acs_handler;

}}}
