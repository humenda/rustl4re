// SPDX-License-Identifier: GPL-2.0-only or LicenseRef-kk-custom
/*
 * Copyright (C) 2019-2020  Kernkonzept GmbH.
 * Author: Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

/**
 * PCIe introduced a technology named PCI Access Control Services (PCI/ACS).
 * The idea is to have a common interface to restrict communication between
 * PCI devices.
 *
 * PCI ACS is optional, but some Intel PCHs without PCI ACS implement features
 * that allow for similar isolation.
 *
 * The purpose of this driver is to enable these features for L4Re.
 *
 * Unfortunately Intel does not officially disclose which PCHs are supported.
 * The most official list of supported PCI IDs is included in Redhat Bugzilla
 * 1037684 (https://bugzilla.redhat.com/show_bug.cgi?id=1037684).
 *
 * The datasheet for 7-series/c216 PCH is here:
 * https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/7-series-chipset-pch-datasheet.pdf
 *
 * The BSRP and UPDCR registers are documented here:
 * https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/4th-gen-core-family-mobile-i-o-datasheet.pdf
 * (page 325)
 */

#include <l4/drivers/hw_mmio_register_block>

#include "cfg.h"
#include "res.h"
#include "debug.h"
#include <pci-driver.h>

namespace {

using namespace Hw::Pci;

struct Intel_root_complex_p2p_inhibitor : Driver
{
  enum Lpc_register
  {
    Rcba        = 0xf0, ///< root complex base address register
    Rcba_enable = 1,
    Rcba_mask   = 0xffffc000,
  };

  enum Chipset_register
  {
    Bspr        = 0x1104, ///< backbone scratch pad register
    Bspr_bpnpd  = (1 << 8), ///< backbone peer non-posted disable
    Bspr_bppd   = (1 << 9), ///< backbone peer posted disable

    Updcr       = 0x1014, ///< upstream peer decode configuration register
    Updcr_enable_mask = 0x3f, ///< peer decode enable bits
  };

  Intel_root_complex_p2p_inhibitor()
  {
    // this matches to LPC controller PCI IDs corresponding to the chipsets
    // that are documented to support the feature in Redhat Bugzilla 1037684
    // 5 series lpc controller
    for (unsigned id = 0x3b00; id <= 0x3b16; ++id)
      register_driver(0x8086, id);
    // 6 series lpc controller
    for (unsigned id = 0x1c40; id <= 0x1c5f; ++id)
      register_driver(0x8086, id);
    // 7 series chipset lpc controller
    for (unsigned id = 0x1e40; id <= 0x1e5f; ++id)
      register_driver(0x8086, id);
    // c600/x79 lpc controller
    for (unsigned id = 0x1d40; id <= 0x1d41; ++id)
      register_driver(0x8086, id);
    // 8 series lpc controller
    for (unsigned id = 0x8c40; id <= 0x8c5f; ++id)
      register_driver(0x8086, id);
    // 9 series lpc controller
    for (unsigned id = 0x9cc0; id <= 0x9cc6; ++id)
      register_driver(0x8086, id);
  }

  /**
   * This function configures the root complex such that requests are always
   * routed through the root complex and that there are no
   * peer-to-peer-requests going directly between root ports. This is the
   * equivalent to Request Redirect, Completion Redirect and Upstream
   * Forwarding in PCI ACS. For more information please consult the PCIe base
   * specification (revision 3).
   */
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG, "Intel PCIe: disable P2P traffic.\n");

    l4_uint32_t rcba;
    d->cfg_read(Rcba, &rcba, Cfg_long);

    if (!(rcba & Rcba_enable))
      {
        d_printf(DBG_DEBUG, "LPC root complex base not enabled\n");
        return 0;
      }

    rcba &= Rcba_mask;
    l4_addr_t vbase = res_map_iomem(rcba, 8192);

    if (!vbase)
      {
        d_printf(DBG_ERR, "Could not map root complex base register space\n");
        return 0;
      }

    auto regs = L4drivers::Mmio_register_block<32>(vbase);
    l4_uint32_t bspr = regs.read<l4_uint32_t>(Bspr);
    d_printf(DBG_DEBUG, "Bspr = %x\n", bspr);
    bspr &= Bspr_bpnpd | Bspr_bppd;
    // If Bspr_bppd (Backbone Peer Posted Disable) and Bspr_bpnpd (Backbone
    // Peer Non-Posted Disable) are both set, we do not need to disable peer
    // decodes on individual root ports (Updcr register)
    if (bspr != (Bspr_bpnpd | Bspr_bppd))
      {
        l4_uint32_t updcr = regs.read<l4_uint32_t>(Updcr);
        d_printf(DBG_DEBUG, "Disabling Updcr peer decodes\n");
        // This setting disables peer requests on all root ports.
        updcr &= ~ Updcr_enable_mask;
        regs.write(updcr, Updcr);
      }
    // TODO unmap?
    return 0;
  }
};

static Intel_root_complex_p2p_inhibitor _intel_root_complex_p2p_inhibitor;

struct Intel_root_port_p2p_inhibitor : Driver
{
  enum Pci_cfg_register
  {
    ///< Miscellaneous Port Configuration Register, see datasheet page 851
    Pci_mpc_register = 0xd8,
  };

  enum Mpc_register
  {
    Irbnce = (1 << 26),  ///< Invalid Receive Bus Number Check Enable
  };

  Intel_root_port_p2p_inhibitor()
  {
    // PCI IDs taken from Redhat Bugzilla 1037684
    // ibexpeak PCH
    // https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/5-chipset-3400-chipset-datasheet.pdf
    for (unsigned id = 0x3b42; id <= 0x3b51; ++id)
      register_driver(0x8086, id);

    // cougarpoint PCH
    // intel 6 series
    // https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/6-chipset-c200-chipset-datasheet.pdf
    for (unsigned id = 0x1c10; id <= 0x1c1f; ++id)
      register_driver(0x8086, id);

    for (unsigned id = 0x1e10; id <= 0x1e1f; ++id)
      register_driver(0x8086, id);

    // lynxpoint-H PCH
    // https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/7-series-chipset-pch-datasheet.pdf
    for (unsigned id = 0x8c10; id <= 0x8c1f; ++id)
      register_driver(0x8086, id);

    // lynxpoint-LP PCH
    // https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/8-series-chipset-pch-datasheet.pdf
    // https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/4th-gen-core-family-mobile-i-o-datasheet.pdf
    for (unsigned id = 0x9c10; id <= 0x9c1b; ++id)
      register_driver(0x8086, id);

    // wildcatpoint PCH
    // series 9
    for (unsigned id = 0x9c90; id <= 0x9c9b; ++id)
      register_driver(0x8086, id);

    // X79
    for (unsigned id = 0x1d10; id <= 0x1d1e; id += 2)
      register_driver(0x8086, id);
  }

  /**
   * This function configures the PCI root port such that it implements the
   * equivalent of PCI ACS source validation. For reference, please consult
   * the PCIe base specification (revision 3).
   */
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG, "Intel PCIe: disable P2P traffic.\n");

    l4_uint32_t mpc;

    d->cfg_read(Pci_mpc_register, &mpc, Cfg_long);

    if (!(mpc & Irbnce))
      {
        d_printf(DBG_DEBUG, "PCIe root port: Setting Irbnce\n");
        mpc |= Irbnce;
        d->cfg_write(Pci_mpc_register, mpc, Cfg_long);
      }
    return 0;
  };
};

static Intel_root_port_p2p_inhibitor _intel_root_port_p2p_inhibitor;

}
