/*!
 * \file
 * \brief  Support for Exynos platforms
 *
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
#include "startup.h"
#include <l4/drivers/uart_s3c2410.h>

#include <stdio.h>

namespace {
class Platform_arm_exynos : public Platform_single_region_ram
{
public:
  bool probe() { return true; }

  void init()
  {
    static L4::Uart_s5pv210 _uart;
    const unsigned long uart_offset = 0x10000;
    unsigned long uart_base;
    unsigned uart_nr = PLATFORM_UART_NR;

    kuart.baud         = 115200;

#ifdef PLATFORM_TYPE_exynos4
    uart_base = 0x13800000;
    kuart.irqno = 52 + 32 + uart_nr; // ext GIC
#if 0
    kuart.irqno = 96 + 8 * 26 + uart_nr; // internal GIC
#endif
#else
    uart_base = 0x12c00000;
    kuart.irqno = 51 + 32 + uart_nr;
#endif

    kuart.base_address = uart_base + uart_nr * uart_offset;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void reboot()
  {
#ifdef PLATFORM_TYPE_exynos4
    *(unsigned *)0x10020400 = 1;
#else
    *(unsigned *)0x10040400 = 1;
#endif
  }

};
}

REGISTER_PLATFORM(Platform_arm_exynos);
