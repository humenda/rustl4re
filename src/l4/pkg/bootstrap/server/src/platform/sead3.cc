/*
 * \brief  Support for the sead3 platform
 *
 * Copyright (C) 2013 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 *
 * Copyright (C) 2015 Kernkonzept GmbH
 * Author: Adam Lackorzynski <adam@l4re.org>
 *
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <l4/drivers/uart_16550.h>
#include "support.h"
#include "macros.h"
#include "startup.h"
#include "mips-defs.h"

namespace {

class Platform_mips_sead3 : public Platform_single_region_ram
{
public:
  bool probe() { return true; }

  void init()
  {
    switch (PLATFORM_UART_NR)
      {
	case 0:
	default:
	  kuart.base_baud    = 921600;
	  kuart.base_address = 0x1f000900;
	  kuart.reg_shift    = 2;
	  kuart.irqno        = 11; // GIC, 4 without GIC
	  break;
	case 1:
	  kuart.base_baud    = 921600;
	  kuart.base_address = 0x1f000800;
	  kuart.reg_shift    = 2;
	  kuart.irqno        = 10; // GIC, 4 without GIC
	  break;
      }

    kuart.baud = 115200;

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 8, 0);
    static L4::Io_register_block_mmio r(kuart.base_address + Mips::KSEG1,
                                        kuart.reg_shift);

    _uart.startup(&r);
    _uart.change_mode(L4::Uart_16550::MODE_8N1, kuart.baud);
    set_stdio_uart(&_uart);

    kuart.access_type  = L4_kernel_options::Uart_type_mmio;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud
                         | L4_kernel_options::F_uart_irq;
  }

  l4_uint64_t to_phys(l4_addr_t bootstrap_addr)
  { return bootstrap_addr - Mips::KSEG0; }

  l4_addr_t to_virt(l4_uint64_t phys_addr)
  { return phys_addr + Mips::KSEG0; }

  void reboot()
  {
    L4::Io_register_block_mmio r(0xbf000000);
    enum { SOFTRES_REGISTER = 0x50, GORESET = 0x4d };
    r.write32(SOFTRES_REGISTER, GORESET);
  }

  const char *get_platform_name()
  {
    return "sead3";
  }
};

}

REGISTER_PLATFORM(Platform_mips_sead3);
