/*!
 * \file
 * \brief  Support for Marvell Armada 38x
 *
 * \date   2015
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2015 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_armada38x : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0xf1012000;
    kuart.base_baud    = 15625000;
    kuart.baud         = 115200;
    kuart.irqno        = 44;
    kuart.reg_shift    = 2;
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_armada38x);
