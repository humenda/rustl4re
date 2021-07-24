// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019-2020 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 */

#pragma once

namespace Pcie_rcar3_regs {

enum
{
  Pcie_car     = 0x0010,              // configuration transmission address
  Pcie_cctlr   = 0x0018,              // configuration transmission control
  Pcie_cctlr_ccie     = 1U << 31,
  Pcie_cctlr_type     = 1 << 8,
  Pcie_cdr     = 0x0020,              // configuration transmission data
  Pcie_msr     = 0x0028,              // mode setting
  Pcie_msr_endpoint   = 0 << 0,
  Pcie_msr_rootport   = 1 << 0,
  Pcie_intxr   = 0x0400,              // INTx register
  Pcie_physr   = 0x07f0,              // physical layer status
  Pcie_msitxr  = 0x0840,              // MSI transmission
  Pcie_msitxr_msie = 1U << 31,
  Pcie_msitxr_mmenum_shft = 16,
  Pcie_tctlr   = 0x2000,              // transfer control register
  Pcie_tctlr_initstrt = 0 << 0,       //   start initialization
  Pcie_tctlr_initdone = 1 << 0,       //   configuration initialization done
  Pcie_tstr    = 0x2004,              // transfer status register
  Pcie_tstr_dllact    = 1 << 0,
  Pcie_intr    = 0x2008,              // interrupt register
  Pcie_errfr   = 0x2020,              // error factor register
  Pcie_errfr_rcvurcpl = 1 << 4,
  Pcie_msialr  = 0x2048,              // MSI address lower register
  Pcie_msiaur  = 0x204c,              // MSI address upper register
  Pcie_msiier  = 0x2050,              // MSI interrupt enable register
  Pcie_prar0   = 0x2080,              // root port address register 0
  Pcie_prar1   = 0x2084,              // root port address register 1
  Pcie_prar2   = 0x2088,              // root port address register 2
  Pcie_prar3   = 0x208c,              // root port address register 3
  Pcie_prar4   = 0x2090,              // root port address register 4
  Pcie_prar5   = 0x2094,              // root port address register 5
  Pcie_lar0    = 0x2200,              // local address register 0
  Pcie_lamr0   = 0x2208,              // local address mask register 0
  Pcie_lamr_16        = 0 << 4,
  Pcie_lamr_32        = ((1 << 1) - 1) << 4,
  Pcie_lamr_64        = ((1 << 2) - 1) << 4,
  Pcie_lamr_128       = ((1 << 3) - 1) << 4,
  Pcie_lamr_256       = ((1 << 4) - 1) << 4,
  Pcie_lamr_512       = ((1 << 5) - 1) << 4,
  Pcie_lamr_256mb     = ((1 << 24) - 1) << 4,
  Pcie_lamr_512mb     = ((1 << 25) - 1) << 4,
  Pcie_lamr_1gb       = ((1 << 26) - 1) << 4,
  Pcie_lamr_2gb       = ((1 << 27) - 1) << 4,
  Pcie_lamr_4gb       = ((1 << 28) - 1) << 4,
  Pcie_lamr_io        = 1 << 0,
  Pcie_lamr_mmio      = 0 << 0,
  Pcie_lamr_laren     = 1 << 1,
  Pcie_lamr_mmio32bit = 0 << 2,
  Pcie_lamr_mmio64bit = 1 << 2,
  Pcie_lamr_mmiopref  = 1 << 3,
  Pcie_lamr_mmionpref = 0 << 3,
  Pcie_lamr_io16b     = 1 << 3,
  Pcie_lamr_io8b      = 1 << 2,
  Pcie_lamr_io4b      = 0 << 2,
  Pcie_lar1    = 0x2220,              // local address register 1
  Pcie_lamr1   = 0x2228,              // local address mask register 1
  Pcie_lar2    = 0x2240,              // local address register 2
  Pcie_lamr2   = 0x2248,              // local address mask register 2
  Pcie_lar3    = 0x2260,              // local address register 3
  Pcie_lamr3   = 0x2268,              // local address mask register 3

  Pcie_palr0   = 0x3400,              // PCIEC address lower register 0
  Pcie_paur0   = 0x3404,              // PCIEC address upper register 0
  Pcie_pamr0   = 0x3408,              // PCIEC address mask register 0
  Pcie_ptctlr0 = 0x340c,              // PCIEC conversion control register 0
  Pcie_ptctlr_pare    = 1 << 31,
  Pcie_ptctlr_spcio   = 1 << 8,
  Pcie_ptctlr_spcmmio = 0 << 8,
  Pcie_palr1   = 0x3420,              // PCIEC address lower register 1
  Pcie_paur1   = 0x3424,              // PCIEC address upper register 1
  Pcie_pamr1   = 0x3428,              // PCIEC address mask register 1
  Pcie_ptctlr1 = 0x342c,              // PCIEC conversion control register 1
  Pcie_palr2   = 0x3440,              // PCIEC address lower register 2
  Pcie_paur2   = 0x3444,              // PCIEC address upper register 2
  Pcie_pamr2   = 0x3448,              // PCIEC address mask register 2
  Pcie_ptctlr2 = 0x344c,              // PCIEC conversion control register 2
  Pcie_palr3   = 0x3460,              // PCIEC address lower register 3
  Pcie_paur3   = 0x3464,              // PCIEC address upper register 3
  Pcie_pamr3   = 0x3468,              // PCIEC address mask register 3
  Pcie_ptctlr3 = 0x346c,              // PCIEC conversion control register 3

  // PCI configuration registers (of the root bridge). See also PCI spec
  //  "Configuration Space" but the RCar3 boards add more registers / bits.
  Pciconf0     = 0x10000,             // vendor ID + device ID
  Pciconf1     = 0x10004,             // command + status
  Pciconf1_parerr     = 1 << 31,      //  detected parity error
  Pciconf1_err        = 1 << 30,      //  signaled system error
  Pciconf1_rma        = 1 << 29,      //  received master abort
  Pciconf1_rta        = 1 << 28,      //  received target abort
  Pciconf1_sta        = 1 << 27,      //  signaled target abort
  Pciconf1_mdpe       = 1 << 24,      //  master data parity error
  Pciconf1_fbtc       = 1 << 23,      //  fast back-to-back transaction cpbl
  Pciconf2     = 0x10008,             // revision ID + class code
  Pciconf3     = 0x1000c,             // cache line size, latency timer etc
  Pciconf4     = 0x10010,             // base address register 0
  Pciconf5     = 0x10014,             // base address register 1
  Pciconf6     = 0x10018,             // primary/secondary/subordinate busnr
  Pciconf7     = 0x1001c,             // IO base/limit + secondary status
  Pciconf8     = 0x10020,             // memory base / limit
  Pciconf9     = 0x10024,             // prefetchable mem base / limit
  Pciconf10    = 0x10028,             // prefetchable mem base upper 32-bit
  Pciconf11    = 0x1002c,             // prefetchable mem limit upper 32-bit
  Pciconf12    = 0x10030,             // IO base/limit upper 16-bit
  Pciconf13    = 0x10034,             // capabilities pointer
  Pciconf14    = 0x10038,             // expansion ROM
  Pciconf15    = 0x1003c,             // int line + int pin + bridge control

  // PCI power management
  Pmcap0       = 0x10040,
  Pmcap1       = 0x10044,

  // MSI capability registers
  Msicap0      = 0x10050,
  Msicap1      = 0x10054,
  Msicap2      = 0x10058,
  Msicap3      = 0x1005c,
  Msicap4      = 0x10060,
  Msicap5      = 0x10064,

  // PCIe capability registers
  Expcap0      = 0x10070,
  Expcap1      = 0x10074,
  Expcap2      = 0x10078,
  Expcap3      = 0x1007c,
  Expcap4      = 0x10080,
  Expcap5      = 0x10084,
  Expcap6      = 0x10088,
  Expcap7      = 0x1008c,
  Expcap8      = 0x10090,
  Expcap9      = 0x10094,
  Expcap10     = 0x10098,
  Expcap11     = 0x1009c,
  Expcap12     = 0x100a0,
  Expcap13     = 0x100a4,
  Expcap14     = 0x100a8,
  // VC capability registers
  Vccap0       = 0x10100,
  Vccap1       = 0x10104,
  Vccap2       = 0x10108,
  Vccap3       = 0x1010c,
  Vccap4       = 0x10110,
  Vccap5       = 0x10114,
  Vccap6       = 0x10118,
  // PCIEC link layer control registers
  Idsetr0      = 0x11000,             // ID setting register 0
  Idsetr1      = 0x11004,             // ID setting register 1
  Tlctlr       = 0x11048,             // TL control register
};

} // namespace Pcie_rcar3_regs
