/*!
 * \file   support_kirkwood.cc
 * \brief  Support for the kirkwood platform
 *
 * \date   2010-11
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2010 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include "mmio_16550.h"

namespace {
class Platform_arm_kirkwood : public Platform_single_region_ram
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0xf1012000; /* uart 1: 0xf1012100 */
    kuart.reg_shift    = 2;
    kuart.base_baud    = 12500000;
    kuart.baud         = 115200;
    kuart.irqno        = 33;         /* uart 1 irq: ??? */
    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 0, 0);
    setup_16550_mmio_uart(&_uart);


    // SPI init, as there's an interrupt pending when coming from u-boot on
    // the dockstar, so make it go away
    *(unsigned *)0xf1010600 = 0; // disable
    *(unsigned *)0xf1010614 = 0; // mask interrupt
    *(unsigned *)0xf1010610 = 1; // clear interrupt
  }
};
}

REGISTER_PLATFORM(Platform_arm_kirkwood);
