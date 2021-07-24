/**
 * \file   support_om.cc
 * \brief  Support for the OpenMoko platform
 *
 * \date   2008
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
#include "startup.h"

#include <l4/drivers/uart_s3c2410.h>
#include <l4/drivers/uart_dummy.h>

namespace {
class Platform_arm_om : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0x50000000;
    kuart.baud = 115200;
    kuart.irqno = 28;
    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;

    static L4::Uart_s3c2410 _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }
};
}

REGISTER_PLATFORM(Platform_arm_om);
