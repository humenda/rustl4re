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
#include <l4/drivers/uart_s3c2410.h>

#include <cstdio>

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

#ifdef PLATFORM_TYPE_exynos4
    uart_base = 0x13800000;
#else
    uart_base = 0x12c00000;
#endif

    static L4::Io_register_block_mmio r(uart_base + uart_nr * uart_offset);
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
