/*!
 * \file   support_pxa.cc
 * \brief  Support for the PXA platform
 *
 * \date   2008-01-04
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "mmio_16550.h"
#include "support.h"

namespace {
class Platform_arm_pxa : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0x40100000;
    kuart.reg_shift    = 2;
    kuart.base_baud    = L4::Uart_16550::Base_rate_pxa;
    kuart.baud         = 115200;
    kuart.irqno        = -1;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 1 << 6, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_pxa);
