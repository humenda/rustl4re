/*
 * \brief  Support for the MIPS Boston platform
 *
 * Copyright (C) 2017 Kernkonzept GmbH
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
class Platform_mips_boston :
  public Platform_base,
  public Boot_modules_image_mode
{
public:
  bool probe() { return true; }

  void init()
  {
    kuart.base_baud    = 1562500;
    kuart.reg_shift    = 2;
    kuart.baud         = 115200;
    kuart.base_address = 0x17ffe000;
    kuart.irqno        = 3;

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
    L4::Io_register_block_mmio plat(Mips::KSEG1 + 0x17ffd000);
    plat.write<l4_uint32_t>(0x10, 1 << 4);
  }

  const char *get_platform_name()
  {
    return "Boston";
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    L4::Io_register_block_mmio plat(Mips::KSEG1 + 0x17ffd000);
    l4_uint32_t ddrcfg = plat.read<l4_uint32_t>(0x38);
    l4_uint32_t ramsize_gb = ddrcfg & 0xf;

    printf("Boston board with %dGB of RAM.\n", ramsize_gb);

    mem_manager->ram->add(Region::start_size(0, 0x10000000, ".ram",
                                             Region::Ram));

    mem_manager->ram->add(Region::start_size(0x90000000,
                                             (ramsize_gb << 30) - 0x10000000,
                                             ".ram", Region::Ram));
  }
};

}

REGISTER_PLATFORM(Platform_mips_boston);
