/*!
 * \file
 * \brief  Support for NXP/Freescale LS1021A and LS1012A
 *
 * \date   2015-2017
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2015-2017 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_ls10xxa : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    switch (PLATFORM_UART_NR)
      {
      default:
      case 0:
        kuart.base_address = 0x21c0500;
        break;
      case 1:
        kuart.base_address = 0x21c0600;
        break;
#if defined(PLATFORM_TYPE_ls1021atwr)
      case 2:
        kuart.base_address = 0x21d0500;
        break;
      case 3:
        kuart.base_address = 0x21d0600;
        break;
#endif
      };

    // Two UARTs share an IRQ
#if defined(PLATFORM_TYPE_ls1021atwr)
    kuart.base_baud    = 9375000;
    kuart.irqno        = 118 + (PLATFORM_UART_NR / 2);
#elif defined (PLATFORM_TYPE_ls1012afrdm)
    kuart.base_baud    = 7812500;
    kuart.irqno        = 86;
#else
#error "Unknown NXP/Freescale Layerscape platform"
#endif

    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    static L4::Uart_16550 _uart(kuart.base_baud);
    setup_16550_mmio_uart(&_uart);
  }

  void reboot()
  {
    L4::Io_register_block_mmio r(0x02ad0000);
    r.write16(0x0, 1 << 2);

    while (1)
      ;
  }
};
}

REGISTER_PLATFORM(Platform_arm_ls10xxa);
