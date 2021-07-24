// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Frank Mehnert <frank.mehnert@kernkonzept.com>
 *            Matthias Lange <matthias.lange@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/drivers/hw_mmio_register_block>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/util/util.h>

#include "debug.h"
#include "res.h"

/*
 * Driver for the RCar-3 clock pulse generator (CPG). Documentation can be
 * found at section 8 ("Clock Pulse Generator") in the Renesas RCar-3 hardware
 * manual. This code is used to enable a hardware unit at the board. If such a
 * unit is not enabled then it's invisible.
 */
class Rcar3_cpg
{
public:
  Rcar3_cpg(l4_addr_t base)
  {
    _base = res_map_iomem(base, 0x1000);
    if (!_base)
      {
        d_printf(DBG_ERR, "ERROR: rcar3_cpg: could not map CPG memory.\n");
        throw -L4_ENOMEM;
      }
    _regs = new L4drivers::Mmio_register_block<32>(_base);
  }

  ~Rcar3_cpg()
  {
    L4Re::Env::env()->rm()->detach(_base, 0);
  }

  /**
   * Enable the clock for a certain hardware unit.
   *
   * \param  n      The number of the module register.
   * \param  bit    The nth bit of the module register.
   *
   * \retval false  Enabling the unit failed for some reason.
   * \retval true   Enabling the unit succeeded.
   *
   * The unit is described by the number of the module register and by the bit
   * within the module register. The bits of all module register are documented
   * in the Renesas RCar-3 hardware manual, see "Module Stop Status Register".
   * If the function returns successfully then the hardware unit was indeed
   * enabled.
   */
  int enable_clock(unsigned n, unsigned bit)
  {
    if (n > Nr_modules - 1)
      {
        d_printf(DBG_ERR, "rcar3_cpg: invalid module %u.\n", n);
        return -L4_EINVAL;
      }
    if (bit > 31)
      {
        d_printf(DBG_ERR, "rcar3_cpg: invalid bit %u.\n", bit);
        return -L4_EINVAL;
      }

    unsigned mask = 1 << bit;

    // assume Cpgwpcr.wpe=1
    unsigned val = _regs[smstpcr[n]];
    val &= ~mask;
    _regs[Cpgwpr] = ~val;
    _regs[smstpcr[n]] = val;

    // The MSTPSRn register shows the status of the corresponding module which
    // was enabled using the respective SMSTPCRn register.
    for (unsigned i = 20; i > 0; --i)
      {
        if (!(_regs[mstpsr[n]] & mask))
          return L4_EOK;
        l4_sleep(5);
      }

    // Device not there or doesn't work.
    return -L4_ENXIO;
  }

private:
  enum : unsigned
  {
    Cpgwpr        = 0x900,      //< CPG write protect register
    Cpgwpcr       = 0x904,      //< CPG write protect control register

    Nr_modules    =    12,      //< Number of module registers
                                //< (for Mstpsr, Rmstpcr, Smstpcr, Scmstpcr)

    Smstpcr0      = 0x130,      //< System module stop control register 0
    Smstpcr1      = 0x134,      //< System module stop control register 1
    Smstpcr2      = 0x138,      //< System module stop control register 2
    Smstpcr3      = 0x13c,      //< System module stop control register 3
    Smstpcr4      = 0x140,      //< System module stop control register 4
    Smstpcr5      = 0x144,      //< System module stop control register 5
    Smstpcr6      = 0x148,      //< System module stop control register 6
    Smstpcr7      = 0x14c,      //< System module stop control register 7
    Smstpcr8      = 0x990,      //< System module stop control register 8
    Smstpcr9      = 0x994,      //< System module stop control register 9
    Smstpcr10     = 0x998,      //< System module stop control register 10
    Smstpcr11     = 0x99c,      //< System module stop control register 11

    Mstpsr0       = 0x030,      //< Module stop status register 0
    Mstpsr1       = 0x038,      //< Module stop status register 1
    Mstpsr2       = 0x040,      //< Module stop status register 2
    Mstpsr3       = 0x048,      //< Module stop status register 3
    Mstpsr4       = 0x04c,      //< Module stop status register 4
    Mstpsr5       = 0x03c,      //< Module stop status register 5
    Mstpsr6       = 0x1c0,      //< Module stop status register 6
    Mstpsr7       = 0x1c4,      //< Module stop status register 7
    Mstpsr8       = 0x9a0,      //< Module stop status register 8
    Mstpsr9       = 0x9a4,      //< Module stop status register 9
    Mstpsr10      = 0x9a8,      //< Module stop status register 10
    Mstpsr11      = 0x9ac,      //< Module stop status register 11

    Rmstpcr0      = 0x110,      //< Realtime module stop control register 0
    Rmstpcr1      = 0x114,      //< Realtime module stop control register 1
    Rmstpcr2      = 0x118,      //< Realtime module stop control register 2
    Rmstpcr3      = 0x11c,      //< Realtime module stop control register 3
    Rmstpcr4      = 0x120,      //< Realtime module stop control register 4
    Rmstpcr5      = 0x124,      //< Realtime module stop control register 5
    Rmstpcr6      = 0x128,      //< Realtime module stop control register 6
    Rmstpcr7      = 0x12c,      //< Realtime module stop control register 7
    Rmstpcr8      = 0x980,      //< Realtime module stop control register 8
    Rmstpcr9      = 0x984,      //< Realtime module stop control register 9
    Rmstpcr10     = 0x988,      //< Realtime module stop control register 10
    Rmstpcr11     = 0x98c,      //< Realtime module stop control register 11
  };

  static constexpr unsigned rmstpcr[Nr_modules] =
  {
    Rmstpcr0, Rmstpcr1, Rmstpcr2, Rmstpcr3, Rmstpcr4, Rmstpcr5,
    Rmstpcr6, Rmstpcr7, Rmstpcr8, Rmstpcr9, Rmstpcr10, Rmstpcr11,
  };

  static constexpr unsigned smstpcr[Nr_modules] =
  {
    Smstpcr0, Smstpcr1, Smstpcr2, Smstpcr3, Smstpcr4, Smstpcr5,
    Smstpcr6, Smstpcr7, Smstpcr8, Smstpcr9, Smstpcr10, Smstpcr11
  };

  static constexpr unsigned mstpsr[Nr_modules] =
  {
    Mstpsr0, Mstpsr1, Mstpsr2, Mstpsr3, Mstpsr4, Mstpsr5,
    Mstpsr6, Mstpsr7, Mstpsr8, Mstpsr9, Mstpsr10, Mstpsr11,
  };

  l4_addr_t _base;
  L4drivers::Register_block<32> _regs;
};
