/*
 * \brief  Support for the Baikal-T platform
 *
 * Copyright (C) 2016 Kernkonzept GmbH
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
#include "panic.h"
#include "startup.h"
#include "mips-defs.h"

namespace {
class Platform_mips_baikal_t :
  public Platform_base,
  public Boot_modules_image_mode
{
public:
  bool probe() { return true; }

  void init()
  {
    kuart.base_baud = 781250;
    kuart.reg_shift = 2;
    kuart.baud      = 38400;

    switch (PLATFORM_UART_NR)
      {
	case 0:
	default:
	  kuart.base_address = 0x1f04a000;
	  kuart.irqno        = 48;
	  break;
	case 1:
	  kuart.base_address = 0x1f04b000;
	  kuart.irqno        = 49;
	  break;
      }

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 8, 0);
    static L4::Io_register_block_mmio_fixed_width<unsigned>
                 r(kuart.base_address + Mips::KSEG1, kuart.reg_shift);

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
    // TBD
  }

  const char *get_platform_name()
  {
    return "Baikal-T";
  }

  Boot_modules *modules() { return this; }
  void setup_memory_map()
  {
    unsigned long ram = RAM_SIZE_MB;
    if (RAM_BASE != 0)
      panic("ERROR: RAM_BASE must be 0x0 on MIPS\n");

    unsigned long b = ram > 128 ? 128 : ram;
    printf("  Memory 0: 00000000 - %08lx (%ldMB)\n",
           (b << 20) - 1, b);
    mem_manager->ram->add(Region::start_size(0, b << 20, ".ram",
                                             Region::Ram));
    if (b >= ram)
      return;

    // post IO hole memory starts beyond 512MB
    if (ram <= 128)
      return;

    ram -= 128;

    printf("  Memory 1: 20000000 - %08lx (%ldMB)\n",
           0x20000000 + (ram << 20) - 1, ram);

    mem_manager->ram->add(Region::start_size(0x20000000, ram << 20, ".ram",
                                             Region::Ram));

    printf("  Memory total size is %ldMB\n", ram + b);
  }
};

}

REGISTER_PLATFORM(Platform_mips_baikal_t);
