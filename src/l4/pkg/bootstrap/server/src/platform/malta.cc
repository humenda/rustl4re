/*
 * \brief  Support for the malta platform
 *
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Prajna Dasgupta <prajna@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
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

class Platform_mips_malta :
  public Platform_base,
  public Boot_modules_image_mode
{
private:
  enum System_type
  {
    Sysctrler_gt,
    Sysctrler_mips,
  };

  System_type _sys_controller;

public:
  bool probe()
  {
    _sys_controller = Sysctrler_gt;
    return true;
  }

  enum
  {
    I8259A_IRQ_BASE = 0,
    I8259A_IRQ_UART_TTY1 = I8259A_IRQ_BASE + 3,
    I8259A_IRQ_UART_TTY0 = I8259A_IRQ_BASE + 4,
  };

  void init()
  {
    unsigned long uart_base;

    // Or better use PLATFORM_UART_NR here?
    switch (_sys_controller)
    {
      case Sysctrler_mips:
        uart_base = 0x1B0003F8;
        break;
      case Sysctrler_gt:
      default:
        uart_base = 0x180003F8;
        break;
    }
    kuart.base_baud    = L4::Uart_16550::Base_rate_x86;
    kuart.base_address = uart_base; // UART0
    kuart.reg_shift    = 0;
    kuart.irqno        = I8259A_IRQ_UART_TTY0;
    kuart.baud         = 115200;

    static L4::Uart_16550 _uart(kuart.base_baud, 0, 0, 8 /*out2 */, 0);
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
    enum { SOFTRES_REGISTER = 0x500, GORESET = 0x42 };
    r.write32(SOFTRES_REGISTER, GORESET);
  }

  const char *get_platform_name()
  {
    switch (_sys_controller)
    {
    case Sysctrler_mips:
      return "malta";
    case Sysctrler_gt:
      return "qemu";
    }
    return "none";
  }

  Boot_modules *modules() { return this; }
  void setup_memory_map()
  {
    unsigned long ram = RAM_SIZE_MB;
    if (RAM_BASE != 0)
      panic("ERROR: RAM_BASE must be 0x0 on MIPS\n");

    unsigned long b = ram > 256 ? 256 : ram;
    printf("  Memory 0: 00000000 - %08lx (%ldMB)\n",
           (b << 20) - 1, b);
    mem_manager->ram->add(Region::start_size(0, b << 20, ".ram",
                                             Region::Ram));

    // post IO hole memory starts beyond 512MB
    if (ram <= 512)
      return;

    ram -= 512;

    printf("  Memory 1: 20000000 - %08lx (%ldMB)\n",
           0x20000000 + (ram << 20) - 1, ram);

    mem_manager->ram->add(Region::start_size(0x20000000, ram << 20, ".ram",
                                             Region::Ram));

    printf("  Memory total size is %ldMB\n", ram + b);
  }
};

}

REGISTER_PLATFORM(Platform_mips_malta);
