/**
 * \file   support_sparc_leon3.cc
 * \brief  Support for the Sparc LEON3 platform
 *
 * \date   2010
 * \author Björn Döbel <doebel@os.inf.tu-dresden.de>
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

#include <l4/drivers/uart_leon3.h>

#include "support.h"

namespace {
class Platform_leon3 :
  public Platform_base,
  public Boot_modules_image_mode
{
  bool probe() { return true; }
  Boot_modules *modules() { return this; }

  enum {
      LEON3_NUM_DEVICE_INFO = 64,

      LEON3_APBUART               = 0x80000100,

      LEON3_VENDOR_GAISLER        = 0x1,
      LEON3_VENDOR_ESA            = 0x4,
      LEON3_DEVICEID_MCTRL        = 0xF,

      LEON3_AHB_BAR_MASK_SHIFT    = 4,
      LEON3_AHB_BAR_MASK_MASK     = 0xFFF,
      LEON3_AHB_BAR_ADDR_SHIFT    = 20,
      LEON3_AHB_BAR_ADDR_MASK     = 0xFFF,

      LEON3_MEMCFG2               = 0x80000004,
      LEON3_MEMCFG2_SDRAMSZ_SHIFT =  23,
      LEON3_MEMCFG2_SDRAMSZ_MASK  =   7,
      LEON3_MEMCFG2_RAMSZ_SHIFT   =   9,
      LEON3_MEMCFG2_SRAM_DISABLEF =  13,
      LEON3_MEMCFG2_RAMSZ_MASK    = 0xF,

      AHB_MASTER_TABLE      = 0xFFFFF000,
      AHB_SLAVE_TABLE       = 0xFFFFF800,
  };

  long _ram_area_start;
  long _ram_area_size;

  void init()
  {
    static L4::Uart_leon3 _uart;
    static L4::Io_register_block_mmio r(LEON3_APBUART); // XXX hard
    _uart.startup(&r);
    set_stdio_uart(&_uart);

    puts("Scanning AHB masters...");
    unsigned *idx = (unsigned*)AHB_MASTER_TABLE;
    while (*idx != 0)
      {
        check_device(idx);
      }

    puts("Scanning AHB slaves...");
    idx = (unsigned*)AHB_SLAVE_TABLE;
    while (*idx != 0)
      {
        check_device(idx);
      }
  }


  void check_device(unsigned *&ahb_idx)
  {
    short vendor = (*ahb_idx >> 24) & 0xF;
    short dev    = (*ahb_idx >> 12) & 0xFFF;

    /* Special check: default RAM memory controller -> find out where the RAM area lies and
     *                how large it is configured to be */
    if ((vendor == (short)LEON3_VENDOR_ESA) && (dev == (short)LEON3_DEVICEID_MCTRL))
      {
        unsigned bar2_addr = (*(ahb_idx+6) >> LEON3_AHB_BAR_ADDR_SHIFT) & LEON3_AHB_BAR_ADDR_MASK;
        unsigned bar2_mask = (*(ahb_idx+6) >> LEON3_AHB_BAR_MASK_SHIFT) & LEON3_AHB_BAR_MASK_MASK;
        _ram_area_start = bar2_addr << 20;
        _ram_area_size  = ~(bar2_mask << 20);
        printf("RAM AREA: [%08lx - %08lx]\n",
               _ram_area_start, _ram_area_start + _ram_area_size);
      }
    print_device(ahb_idx);
  }

  void print_device(unsigned *&ahb_idx)
  {
    short dev    = (*ahb_idx >> 24) & 0xF;
    short vendor = (*ahb_idx >> 12) & 0xFFF;
    printf("dev:vendor %04hx:%04hx\n", dev, vendor);
    printf("%08x  %08x  %08x  %08x\n", *ahb_idx, *(ahb_idx+1), *(ahb_idx+2), *(ahb_idx+3));
    ahb_idx += 4;
    printf("%08x  %08x  %08x  %08x\n", *ahb_idx, *(ahb_idx+1), *(ahb_idx+2), *(ahb_idx+3));
    ahb_idx += 4;
    printf("--------------------------------------\n");
  }

  void setup_memory_map()
  {
    /* § 10.8.2
       SDRAM area is mapped into the upper half of the RAM area defined by BAR2
       register. When the SDRAM enable bit is set in MCFG2, the controller is
       enabled and mapped into upper half of the RAM area as long as the SRAM
       disable bit is not set. If the SRAM disable bit is set, all access to
       SRAM is disabled and the SDRAM banks are mapped into the lower half of
       the RAM area.
     */
    unsigned mcfg2      = *(unsigned*)LEON3_MEMCFG2;
    unsigned ram_size   = (mcfg2 >> LEON3_MEMCFG2_RAMSZ_SHIFT) & LEON3_MEMCFG2_RAMSZ_MASK;
    unsigned sdram_size = (mcfg2 >> LEON3_MEMCFG2_SDRAMSZ_SHIFT) & LEON3_MEMCFG2_SDRAMSZ_MASK;

    sdram_size = 4 << sdram_size;

    long sdram_base = _ram_area_start;
    if (!(mcfg2 & LEON3_MEMCFG2_SRAM_DISABLEF))
      sdram_base = _ram_area_start + ((_ram_area_size + 1) >> 1);

#ifdef RAM_SIZE_MB
    sdram_size = RAM_SIZE_MB;
    sdram_base = RAM_BASE;
#endif

    printf("RAM:   %4d kB\n", (8192 << ram_size) / 1024);
    printf("SDRAM: %4d MB\n", sdram_size);

    mem_manager->ram->add(Region::n(sdram_base, sdram_base + (sdram_size << 20),
                                    ".sdram", Region::Ram));
  }
};
}

REGISTER_PLATFORM(Platform_leon3);
