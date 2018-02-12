/**
 * \file
 * \brief  Support for the MPC52000
 *
 * \date   2009-02-16
 * \author Sebastian Sumpf <sumpf@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "support.h"
#include <l4/drivers/uart_of.h>
#include <l4/drivers/of_if.h>
#include <l4/drivers/of_dev.h>

namespace {
class Platform_ppc_mpc52000 :
  public Platform_base,
  public Boot_modules_image_mode
{
  bool probe() { return true; }
  Boot_modules *modules() { return this; }

  void boot_kernel(unsigned long entry)
  {
    typedef void (*func)(l4util_mb_info_t *, unsigned long);
    L4_drivers::Of_if of_if;
    of_if.boot_finish();
    ((func)entry)(0, of_if.get_prom());
    exit(-100);
  }

  void init()
  {
    static L4::Uart_of _uart;
    static L4::Io_register_block_mmio r(0);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

  void setup_memory_map()
  {
    L4_drivers::Of_if of_if;

    printf("  Detecting ram size ...\n");
    unsigned long ram_size = of_if.detect_ramsize();
    printf("    Total memory size is %luMB\n", ram_size / (1024 * 1024));
    mem_manager->ram->add(Region::n(0x0, ram_size, ".ram", Region::Ram));

    // FIXME: move this somewhere else, it has mothing to do with
    // memory setup
#if 0
    /* detect OF devices */
    unsigned long drives_addr, drives_length;

    if (of_if.detect_devices(&drives_addr, &drives_length))
      {
        mbi->flags |= L4UTIL_MB_DRIVE_INFO;
        mbi->drives_addr   = drives_addr;
        mbi->drives_length = drives_length;
      }
#endif
  }
};
}

REGISTER_PLATFORM(Platform_ppc_mpc52000);
