/*!
 * \file
 * \brief  Support for Renesas Gen3
 *
 * \date   2016-2017
 * \author Adam Lackorzynski <adam@l4re.org>
 *
 */
/*
 * (c) 2016-2017 Author(s)
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_sh.h>
#include "support.h"
#include "startup.h"

namespace {
class Platform_arm_rcar3 : public Platform_base,
                           public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    kuart.base_address = 0xe6e88000;
    kuart.reg_shift    = 0;
    kuart.base_baud    = 14745600;
    kuart.baud         = 115200;
    kuart.irqno        = 196;
    static L4::Uart_sh _uart;
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    // product register for R-Car Gen3
    l4_uint32_t prr = *(l4_uint32_t*)0xFFF00044;
    l4_uint32_t ufamily = (prr & 0xff00) >> 8;
    const char *sfamily = NULL;
    mem_manager->ram->add(Region(0x048000000, 0x07fffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x600000000, 0x63fffffff, ".ram", Region::Ram));
    switch (ufamily)
      {
      case 0x4f: sfamily = "r8a7795"; break;
      case 0x52: sfamily = "r8a7796"; break;
      }
    if (sfamily)
      {
        char rev[16];
        if ((prr & 0x7fff) == 0x5210)
          snprintf(rev, sizeof(rev), "ES1.1");
        else
          snprintf(rev, sizeof(rev), "ES%u.%u",
                   ((prr >> 4) & 0x0f) + 1, prr & 0xf);

        if (ufamily == 0x4f)
          {
            mem_manager->ram->add(Region(0x500000000, 0x53fffffff, ".ram", Region::Ram));
            mem_manager->ram->add(Region(0x700000000, 0x73fffffff, ".ram", Region::Ram));
          }

        printf("  Found R-Car Gen3 %s %s\n", sfamily, rev);
      }
    else
      printf("Configured for R-Car Gen3 but found unknown product (prr=0x%08x)\n", prr);
  }

  void reboot()
  {
    // Call PSCI-SYSTEM_RESET
    register unsigned long r0 asm("r0") = 0x84000009;
    asm volatile("smc #0" : : "r" (r0));
  }
};
}

REGISTER_PLATFORM(Platform_arm_rcar3);
