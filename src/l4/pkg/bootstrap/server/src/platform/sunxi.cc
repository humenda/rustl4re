
/*!
 * \file
 * \brief  Support for sunxi platforms
 *
 * \date   2013
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2013 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_sunxi : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0x01c28000;
    kuart.reg_shift    = 2;
    kuart.base_baud    = 1500000;
    kuart.baud         = 115200;
    kuart.irqno        = 33;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_sunxi);
