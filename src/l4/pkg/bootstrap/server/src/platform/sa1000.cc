/*!
 * \file   support_sa1000.cc
 * \brief  Support for SA1000 platform
 *
 * \date   2008-01-02
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include <l4/drivers/uart_sa1000.h>

namespace {
class Platform_arm_sa1000 : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    static L4::Uart_sa1000 _uart;
    static L4::Io_register_block_mmio r(0x80010000);
    //static L4::Io_register_block_mmio r(0x80050000);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_sa1000);
