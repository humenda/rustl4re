/*!
 * \file
 * \brief  Support for Marvell Armada 37x0
 *
 * \date   2017
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2017 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_mvebu.h>

#include "startup.h"
#include "support.h"

namespace {
class Platform_arm_armada37xx : public Platform_base,
                                public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0xd0012000;
    kuart.base_baud    = 25804800;
    kuart.baud         = 115200;
    kuart.irqno        = 43;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
    static L4::Uart_mvebu _uart(kuart.base_baud);
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    mem_manager->ram->add(Region(0x00000000, 0x03ffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x04200000, 0x3fffffff, ".ram", Region::Ram));
  }
};
}

REGISTER_PLATFORM(Platform_arm_armada37xx);
