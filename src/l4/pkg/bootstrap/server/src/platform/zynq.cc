/*!
 * \file
 * \brief  Support for Zynq platforms
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

#include <l4/drivers/uart_cadence.h>
#include "startup.h"
#include "support.h"

namespace {
class Platform_arm_zynq : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.baud      = 115200;
    kuart.reg_shift = 0;

#ifdef PLATFORM_TYPE_zynqmp
    kuart.base_baud = 99648000;
    switch (PLATFORM_UART_NR) {
      default:
      case 0: kuart.base_address = 0xff000000;
              kuart.irqno        = 32 + 21;
              break;
      case 1: kuart.base_address = 0xff010000;
              kuart.irqno        = 32 + 22;
              break;
    };
#else
    kuart.base_baud = 5000000;
    switch (PLATFORM_UART_NR) {
      case 0: kuart.base_address = 0xe0000000; // QEMU
              kuart.irqno        = 59;
              break;
      default:
      case 1: kuart.base_address = 0xe0001000; // Zedboard
              kuart.irqno        = 82;
              break;
    };
#endif

    static L4::Uart_cadence _uart(kuart.base_baud);
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    _uart.change_mode(3, 115200);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
    L4::Io_register_block_mmio r(0xf8000200);
    r.write<unsigned>(1, 0);
  }
};
}

REGISTER_PLATFORM(Platform_arm_zynq);
