/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"


namespace {
class Platform_arm_virt : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    // set defaults for reg_shift and baud_rate
    kuart.baud      = 115200;
    kuart.reg_shift = 0;

    kuart.base_address = 0x09000000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 33;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_pl011 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
    printf("ARM Virtual Platform reboot not implemented\n");

    while (1)
      ;
  }
};
}

REGISTER_PLATFORM(Platform_arm_virt);
