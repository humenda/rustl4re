/*!
 * \file   support_beagleboard.cc
 * \brief  Support for the Beagleboard
 *
 * \date   2009-08
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "startup.h"
#include <l4/drivers/uart_omap35x.h>

namespace {
class Platform_arm_omap : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.baud         = 115200;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
#ifdef PLATFORM_TYPE_beagleboard
    kuart.base_address = 0x49020000;
    kuart.irqno = 74;
#elif defined(PLATFORM_TYPE_omap3evm)
    kuart.base_address = 0x4806a000;
    kuart.irqno = 72;
#elif defined(PLATFORM_TYPE_omap3_am33xx)
    kuart.base_address = 0x44e09000;
    kuart.irqno = 72;
#elif defined(PLATFORM_TYPE_pandaboard) || defined(PLATFORM_TYPE_omap5)
    kuart.base_address = 0x48020000;
    kuart.irqno = 32 + 74;
#else
#error Unknown platform
#endif

    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Uart_omap35x _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  bool arm_switch_to_hyp()
  {
    register l4_umword_t f asm("r12") = 0x102;
    asm volatile("push {fp}                 \n"
                 "mcr p15, 0, sp, c13, c0, 2\n"
                 "mov r0, pc                \n"
                 ".inst 0xe1600071          \n"
                 "mrc p15, 0, sp, c13, c0, 2\n"
                 "pop {fp}                  \n"
                 :
                 : "r" (f)
                 : "r0", "r1", "r2", "r3", "r4",
                   "r5", "r6", "r7", "r8", "r9",
                   "r10", "memory");

    return true;
  }
};
}

REGISTER_PLATFORM(Platform_arm_omap);
