/*!
 * \file
 * \brief  Support for Tegra 3 platforms
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
class Platform_arm_tegra3 : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    switch (PLATFORM_UART_NR)
      {
      case 0:
        kuart.base_address = 0x70006000;
        kuart.irqno        = 68;
        break;
      default:
      case 2:
        kuart.base_address = 0x70006200;
        kuart.irqno        = 78;
        break;
      };
    kuart.reg_shift    = 2;
    kuart.base_baud    = 25459200;
    kuart.baud         = 115200;

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_tegra3);
