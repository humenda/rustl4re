/*!
 * \file   support_imx.cc
 * \brief  Support for the i.MX platform
 *
 * \date   2008-02-04
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

#include <l4/drivers/uart_imx.h>
#include <l4/drivers/uart_lpuart.h>
#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "startup.h"


namespace {
class Platform_arm_imx
#if defined(PLATFORM_TYPE_imx8x) || defined(PLATFORM_TYPE_imx8q)
  : public Platform_base, public Boot_modules_image_mode
#else
  : public Platform_single_region_ram
#endif
{
  bool probe() { return true; }

  void init()
  {
    _wdog_phys = 0;

    // set defaults for reg_shift and baud_rate
    kuart.baud      = 115200;
    kuart.reg_shift = 0;
    kuart.access_type = L4_kernel_options::Uart_type_mmio;
    kuart_flags |= L4_kernel_options::F_uart_base
                   | L4_kernel_options::F_uart_baud
                   | L4_kernel_options::F_uart_irq;

#ifdef PLATFORM_TYPE_imx21
    kuart.base_address = 0x1000A000;
    kuart.irqno        = 20;
    static L4::Uart_imx21 _uart;
#elif defined(PLATFORM_TYPE_imx28)
    kuart.base_address = 0x80074000;
    kuart.base_baud    = 23990400;
    kuart.irqno        = 47;
    static L4::Uart_pl011 _uart(kuart.base_baud);
#elif defined(PLATFORM_TYPE_imx35)
    static L4::Uart_imx35 _uart;
    switch (PLATFORM_UART_NR) {
      default:
      case 1: kuart.base_address = 0x43f90000;
              kuart.irqno        = 45;
              break;
      case 2: kuart.base_address = 0x43f94000;
              kuart.irqno        = 32;
              break;
      case 3: kuart.base_address = 0x5000c000;
              kuart.irqno        = 18;
              break;
    }
    _wdog_phys = 0x53fdc000;
#elif defined(PLATFORM_TYPE_imx51)
    kuart.base_address = 0x73fbc000;
    kuart.irqno = 31;
    _wdog_phys = 0x73f98000;
    static L4::Uart_imx51 _uart;
#elif defined(PLATFORM_TYPE_imx53)
    kuart.base_address = 0x53fbc000;
    kuart.irqno = 31;
    _wdog_phys = 0x53f98000;
    static L4::Uart_imx51 _uart;
#elif defined(PLATFORM_TYPE_imx6) || defined(PLATFORM_TYPE_imx6ul)
    switch (PLATFORM_UART_NR) {
      case 1: kuart.base_address = 0x02020000;
              kuart.irqno        = 58;
              break;
      default:
      case 2: kuart.base_address = 0x021e8000;
              kuart.irqno        = 59;
              break;
      case 3: kuart.base_address = 0x021ec000;
              kuart.irqno        = 60;
              break;
      case 4: kuart.base_address = 0x021f0000;
              kuart.irqno        = 61;
              break;
      case 5: kuart.base_address = 0x021f4000;
              kuart.irqno        = 62;
              break;
    };
    _wdog_phys = 0x020bc000;
    static L4::Uart_imx6 _uart;
#elif defined(PLATFORM_TYPE_imx7)
    switch (PLATFORM_UART_NR) {
      default:
      case 1: kuart.base_address = 0x30860000;
              kuart.irqno        = 32 + 26;
              break;
      case 2: kuart.base_address = 0x30870000;
              kuart.irqno        = 32 + 27;
              break;
      case 3: kuart.base_address = 0x30880000;
              kuart.irqno        = 32 + 28;
              break;
      case 4: kuart.base_address = 0x30a60000;
              kuart.irqno        = 32 + 29;
              break;
      case 5: kuart.base_address = 0x30a70000;
              kuart.irqno        = 32 + 30;
              break;
      case 6: kuart.base_address = 0x30a80000;
              kuart.irqno        = 32 + 16;
              break;
    };
    _wdog_phys = 0x30280000;
    static L4::Uart_imx7 _uart;
#elif defined(PLATFORM_TYPE_imx8m)
    switch (PLATFORM_UART_NR) {
      default:
      case 1: kuart.base_address = 0x30860000;
              kuart.irqno        = 32 + 26;
              break;
      case 2: kuart.base_address = 0x30890000;
              kuart.irqno        = 32 + 27;
              break;
      case 3: kuart.base_address = 0x30880000;
              kuart.irqno        = 32 + 28;
              break;
      case 4: kuart.base_address = 0x30a60000;
              kuart.irqno        = 32 + 29;
              break;
    };
    _wdog_phys = 0x30280000;
    static L4::Uart_imx8 _uart;
#elif defined(PLATFORM_TYPE_imx8x) || defined(PLATFORM_TYPE_imx8q)

#ifdef PLATFORM_TYPE_imx8x
    enum { Uart_irq_base = 32 + 225 };
#else
    enum { Uart_irq_base = 32 + 345 };
#endif
    switch (PLATFORM_UART_NR) {
      default:
      case 1: kuart.base_address = 0x5a060000; // LPUART0
              kuart.irqno        = Uart_irq_base;
              break;
      case 2: kuart.base_address = 0x5a070000; // LPUART1
              kuart.irqno        = Uart_irq_base + 1;
              break;
      case 3: kuart.base_address = 0x5a080000; // LPUART2
              kuart.irqno        = Uart_irq_base + 2;
              break;
      case 4: kuart.base_address = 0x5a090000; // LPUART3
              kuart.irqno        = Uart_irq_base + 3;
              break;
    };
    static L4::Uart_lpuart _uart;
#else
#error Which platform type?
#endif
    static L4::Io_register_block_mmio r(kuart.base_address);
    _uart.startup(&r);
    set_stdio_uart(&_uart);
  }

#if defined(PLATFORM_TYPE_imx8x)
  void setup_memory_map()
  {
    mem_manager->ram->add(Region(0x080200000, 0x083ffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x088000000, 0x08fffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x095c00000, 0x0ffffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x880000000, 0x8bfffffff, ".ram", Region::Ram));
  }

  Boot_modules *modules() { return this; }
#elif defined(PLATFORM_TYPE_imx8q)
  enum
  {
    MU              = 0x5d1c0000,
    MU_SR_TE0_MASK1 = 1 << 23,
    MU_SR_RF0_MASK1 = 1 << 27,
    MU_ATR0_OFFSET1 = 0x0,
    MU_ARR0_OFFSET1 = 0x10,
    MU_ASR_OFFSET1  = 0x20,
    MU_ACR_OFFSET1  = 0x24
  };

  L4::Io_register_block_mmio _mu{MU};

  void write_mu(unsigned int regIndex, l4_uint32_t msg)
  {
    unsigned int mask = MU_SR_TE0_MASK1 >> regIndex;

    // Wait for TX to be empty
    while (!(_mu.read32(MU_ASR_OFFSET1) & mask))
      ;
    _mu.write32(MU_ATR0_OFFSET1  + (regIndex * 4), msg);
  }

  void read_mu(unsigned int regIndex, l4_uint32_t *msg)
  {
    unsigned int mask = MU_SR_RF0_MASK1 >> regIndex;

    // Wait for RX to be full
    while (!(_mu.read32(MU_ASR_OFFSET1) & mask))
      ;
    *msg = _mu.read32(MU_ARR0_OFFSET1 + (regIndex * 4));
  }

  struct Scu_msg
  {
    union {
      struct {
        l4_uint8_t ver;
        l4_uint8_t size;
        l4_uint8_t svc;
        l4_uint8_t func;
      } h;
      l4_uint32_t hdr;
    };
    l4_uint32_t data[4];

    enum { Scu_svc_rm = 3 };

    explicit Scu_msg(l4_uint8_t func, l4_uint8_t sz)
    {
      h.ver = 1;
      h.svc = Scu_svc_rm;
      h.size = sz;
      h.func = func;
    }
  };

  void scu_call(Scu_msg *msg, bool has_result)
  {
    // Write header
    write_mu(0, msg->hdr);
    l4_uint8_t count = 1;

    // Write data into the 4 channels
    while (count < msg->h.size)
      {
        write_mu(count % 4 , msg->data[count - 1]);
        count++;
      }

    if (has_result)
      {
        // Read header
        read_mu(0, &msg->hdr);
        count = 1;

        // Read data from the 4 channels
        while (count < msg->h.size)
          {
            read_mu(count % 4, &msg->data[count - 1]);
            count++;
          }
      }
  }

  bool scu_mem_owned(l4_uint8_t mr)
  {
    Scu_msg msg(21, 2);
    msg.data[0] = mr;
    scu_call(&msg, true);
    return (l4_uint8_t)msg.data[0];
  }

  void scu_mem_info(l4_uint8_t mr, l4_uint64_t *s, l4_uint64_t *e)
  {
    Scu_msg msg(22, 2);
    msg.data[0] = mr;
    scu_call(&msg, true);
    *s = msg.data[1] | (l4_uint64_t)msg.data[0] << 32;
    *e = msg.data[3] | (l4_uint64_t)msg.data[2] << 32;
  }

  void setup_memory_map()
  {
    // Find the end of the second dram bank
    l4_uint64_t end = 0x8ffffffff; // Default to 2GB
    for (int mr = 0; mr < 64; ++mr)
      {
        if (!scu_mem_owned(mr))
          continue;

        l4_uint64_t a, b;
        scu_mem_info(mr, &a, &b);
        if (a == 0x880000000)
          {
            if (b == 0xfffffffff)
              end = 0x97fffffff; // 6GB variant
            else if (b <= 0x9ffffffff)
              end = b;
            // Otherwise leave at default

            break;
          }
      }

    mem_manager->ram->add(Region(0x080200000, 0x083ffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x088000000, 0x08fffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x095c00000, 0x0ffffffff, ".ram", Region::Ram));
    mem_manager->ram->add(Region(0x880000000, end, ".ram", Region::Ram));
  }

  Boot_modules *modules() { return this; }
#endif

  void reboot()
  {
    if (_wdog_phys)
      {
        L4::Io_register_block_mmio r(_wdog_phys);
        r.modify<unsigned short>(0, 0xff00, 1 << 2);
      }

    while (1)
      ;
  }
private:
  unsigned long _wdog_phys;
};
}

REGISTER_PLATFORM(Platform_arm_imx);
