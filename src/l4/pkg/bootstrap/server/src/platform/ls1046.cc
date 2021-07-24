/*!
 * \file
 * \brief  Support for NXP LS1046
 *
 * \date   2021
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2021 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_16550.h>
#include "support.h"
#include "startup.h"

namespace {

class Platform_arm_ls1046 : public Platform_base,
                            public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0x21c0500;
    kuart.base_baud    = 0;
    kuart.irqno        = 86;
    kuart.baud         = 115200;
    kuart.reg_shift    = 0;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Io_register_block_mmio r(kuart.base_address);
    static L4::Uart_16550 _uart(kuart.base_baud);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    mem_manager->ram->add(Region(0x080000000, 0x0fbdfffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x880000000, 0x8ffffffff, ".ram", Region::Ram));
  }

  void reboot()
  {
    reboot_psci();
    while (1)
      ;
  }
};
}

REGISTER_PLATFORM(Platform_arm_ls1046);
