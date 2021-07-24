/*!
 * \file   rpi.cc
 * \brief  Support for the Raspberry Pi
 *
 * \date   2013
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2013 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/uart_pl011.h>
#include "support.h"
#include "mmio_16550.h"

namespace {

// No multi-tag handling, although possible with VC
struct Mbox_gen
{
  enum
  {
    Mbox0_read   = 0x00,
    Mbox0_status = 0x18,
    Mbox0_config = 0x1c,
    Mbox1_write  = 0x20,

    Mbox_empty = 1 << 30,
  };

  struct Mbox_hdr
  {
    l4_uint32_t buffer_size;
    l4_uint32_t cmd_and_response;
  };

  struct Mbox_tag_hdr
  {
    l4_uint32_t tag;
    l4_uint32_t buffer_size;
    l4_uint32_t input_buffer_size;
  };

  Mbox_gen(l4_addr_t base, l4_uint32_t tag_id, l4_uint32_t input_buffer_size)
  : _base(base)
  {
    Mbox_hdr *hdr = (Mbox_hdr *)&_m[0];
    hdr->buffer_size      = sizeof(_m);
    hdr->cmd_and_response = 0; // process request

    Mbox_tag_hdr *tag = (Mbox_tag_hdr *)&_m[2];
    tag->tag = tag_id;
    tag->buffer_size = sizeof(_m)
                       - sizeof(l4_uint32_t) // end tag
                       - sizeof(*hdr)
                       - sizeof(*tag);
    tag->input_buffer_size = input_buffer_size;

    memset(&_m[5], 0, sizeof(_m) - 5 * sizeof(_m[0])); // includes end-tag setting
  }

  l4_uint32_t const *response_buffer() const
  { return &_m[5]; }

  void clean_dcache(unsigned long addr)
  {
    asm volatile("dsb sy");
#ifdef ARCH_arm64
    asm volatile("dc cvac, %0" : : "r" (addr) : "memory");
#endif
#ifdef ARCH_arm
    asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr) : "memory");
#endif
  }

  void flush_dcache(unsigned long addr)
  {
    asm volatile("dsb sy");
#ifdef ARCH_arm64
    asm volatile("dc civac, %0" : : "r" (addr) : "memory");
#endif
#ifdef ARCH_arm
    asm volatile("mcr p15, 0, %0, c7, c14, 1 \n"
                 "mcr p15, 0, %0, c7, c5, 1  \n"
                 : : "r" (addr) : "memory");
#endif
  }

  void inv_dcache(unsigned long addr)
  {
    asm volatile("dsb sy");
#ifdef ARCH_arm64
    asm volatile("dc ivac, %0" : : "r" (addr) : "memory");
#endif
#ifdef ARCH_arm
    asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r" (addr) : "memory");
#endif
  }

  bool call()
  {
    unsigned long mboxa = _base + 0xb880;
    L4::Io_register_block_mmio mboxdev(mboxa);
    clean_dcache((unsigned long)_m); // todo: all range...
    unsigned v = (unsigned)(unsigned long)_m |  8;
    mboxdev.write<unsigned>(Mbox1_write, v);
    clean_dcache(mboxa + Mbox1_write);

    while (1)
      {
        inv_dcache(mboxa + Mbox0_read);
        while (mboxdev.read<unsigned>(Mbox0_status) & Mbox_empty)
          inv_dcache(mboxa + Mbox0_read);;

        if (mboxdev.read<unsigned>(Mbox0_read) == v) // our request?
          {
            inv_dcache((unsigned long)_m);
            return _m[1] == (1u << 31); // response
          }
      }
  }

  l4_addr_t _base;
  l4_uint32_t _m[8] __attribute__((aligned(128)));
};

static constexpr const char *type_list[] = {
  "A",
  "B",
  "A+",
  "B+",
  "2B",
  "Alpha",
  "CM1",
  "Unknown",
  "3B",
  "Zero",
  "CM3",
  "Unknown",
  "Zero W",
  "3B+",
  "3A+",
  "Internal use only",
  "CM3+",
  "4B",
  "Unknown",
  "400",
  "CM4",
};

static constexpr const char *processor_list[] = {
  "BCM2835", "BCM2836", "BCM2837", "BCM2711",
};

struct Old_rev
{
  char model[4];
  unsigned char rev;
  enum Mem_old_rev
  {
    Mem_inv,
    Mem_256mb,
    Mem_512mb,
  } mem;
};

static const Old_rev old_revs[] =
{
  { "N/A", 0,            Old_rev::Mem_inv },
  { "N/A", 0,            Old_rev::Mem_inv },
  { "B",   (1 << 4) | 0, Old_rev::Mem_256mb },
  { "B",   (1 << 4) | 0, Old_rev::Mem_256mb },

  { "B",   (2 << 4) | 0, Old_rev::Mem_256mb },
  { "B",   (2 << 4) | 0, Old_rev::Mem_256mb },
  { "B",   (2 << 4) | 0, Old_rev::Mem_256mb },
  { "A",   (2 << 4) | 0, Old_rev::Mem_256mb },

  { "A",   (2 << 4) | 0, Old_rev::Mem_256mb },
  { "A",   (2 << 4) | 0, Old_rev::Mem_256mb },
  { "N/A", 0,            Old_rev::Mem_inv },
  { "N/A", 0,            Old_rev::Mem_inv },

  { "N/A", 0,            Old_rev::Mem_inv},
  { "B",   (2 << 4) | 0, Old_rev::Mem_512mb },
  { "B",   (2 << 4) | 0, Old_rev::Mem_512mb },
  { "B",   (2 << 4) | 0, Old_rev::Mem_512mb },

  { "B+",  (1 << 4) | 2, Old_rev::Mem_512mb },
  { "CM1", (1 << 4) | 0, Old_rev::Mem_512mb },
  { "A+",  (1 << 4) | 1, Old_rev::Mem_256mb },
  { "B+",  (1 << 4) | 2, Old_rev::Mem_512mb },

  { "CM1", (1 << 4) | 0, Old_rev::Mem_512mb },
  { "A+",  (1 << 4) | 1, Old_rev::Mem_256mb },
};

enum { Old_rev_count = sizeof(old_revs) / sizeof(old_revs[0]) };

struct Mbox_boardrev : public Mbox_gen
{
  explicit Mbox_boardrev(l4_addr_t base) : Mbox_gen(base, 0x10002, 0) {}
  l4_uint32_t rev_raw() const { return response_buffer()[0]; }

  unsigned revision() const               { return (rev_raw() >>  0) & 0xf; }
  unsigned type() const                   { return (rev_raw() >>  4) & 0xff; }
  unsigned processor() const              { return (rev_raw() >> 12) & 0xf; }
  unsigned manufacturer() const           { return (rev_raw() >> 16) & 0xf; }
  unsigned memory() const                 { return (rev_raw() >> 20) & 7; }
  bool new_flag() const                   { return (rev_raw() >> 23) & 1; }
  bool warrenty_voided() const            { return (rev_raw() >> 25) & 1; }
  bool otp_reading_disallowed() const     { return (rev_raw() >> 29) & 1; }
  bool otp_programming_disallowed() const { return (rev_raw() >> 30) & 1; }
  bool overvoltage_disallowed() const     { return (rev_raw() >> 31) & 1; }

  const char *type_str() const
  {
    if (new_flag())
      {
        unsigned t = type();
        if (t < sizeof(type_list) / sizeof(type_list[0]))
          return type_list[t];
      }
    else if (rev_raw() <= Old_rev_count)
      return old_revs[rev_raw()].model;
    return "Unknown"; // Update above list
  }

  const char *processor_str() const
  {
    unsigned t = processor();
    if (t < sizeof(processor_list) / sizeof(processor_list[0]))
      return processor_list[t];
    return "Unknown"; // Update above list
  }

  unsigned memory_size_mb() const
  {
    if (new_flag())
      {
        if (memory() > 5) // Non-spec'ed number
          return 256u; // smallest size ever, and this is just informational anyway
        return (256u * (1 << memory()));
      }

    if (rev_raw() <= Old_rev_count)
      if (old_revs[rev_raw()].mem == Old_rev::Mem_512mb)
        return 512u;
    return 256u;
  }

  unsigned char old_rev_encoded()
  {
    if (rev_raw() <= Old_rev_count)
      return old_revs[rev_raw()].rev;
    return 0;
  }
};

struct Mbox_serial : public Mbox_gen
{
  Mbox_serial(l4_addr_t base) : Mbox_gen(base, 0x10004, 0) {}
  l4_uint64_t serial()
  {
    l4_uint32_t const *r = response_buffer();
    return ((l4_uint64_t)r[1] << 32) | r[0];
  }
};

struct Mbox_armmem : public Mbox_gen
{
  Mbox_armmem(l4_addr_t base) : Mbox_gen(base, 0x10005, 0) {}
  l4_uint32_t base() { return response_buffer()[0]; }
  l4_uint32_t size() { return response_buffer()[1]; }
};


class Platform_arm_rpi : public Platform_base,
                         public Boot_modules_image_mode
{
  bool probe() { return true; }

  void init()
  {
    unsigned rpi_ver;
    unsigned long m;

#ifdef ARCH_arm
    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
    switch ((m >> 4) & 0xf00)
      {
      case 0xc00: rpi_ver = 2; break;
      case 0xd00: rpi_ver = ((m >> 4) & 0xf) == 3 ? 3 : 4; break;
      default: rpi_ver = 1; break;
      }
#endif
#ifdef ARCH_arm64
    asm volatile("mrs %0, midr_el1" : "=r" (m));
    if ((m & 0xff0ffff0) == 0x410fd030)
      rpi_ver = 3;
    else
      rpi_ver = 4;
#endif

    switch (rpi_ver)
      {
      case 1:
        _base = 0x20000000;
        kuart.base_address = _base + 0x00201000;
        break;
      case 2:
        _base = 0x3f000000;
        kuart.base_address = _base + 0x00201000;
        break;
      case 3:
        _base = 0x3f000000;
        kuart.base_address = _base + 0x00215040;
        kuart.irqno        = 29;
        kuart.base_baud    = 31250000;
        break;
      case 4:
        _base = 0xfe000000;
        kuart.base_address = _base + 0x00215040;
        kuart.irqno        = 32 + 93; // GIC
        kuart.base_baud    = 62500000;
        break;
      };

    kuart.baud = 115200;

    if (rpi_ver == 1 || rpi_ver == 2)
      {
        kuart.base_baud  = 0;
        kuart.irqno      = 57;
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart_flags       |=   L4_kernel_options::F_uart_base
                             | L4_kernel_options::F_uart_baud
                             | L4_kernel_options::F_uart_irq;
        static L4::Io_register_block_mmio r(kuart.base_address);
        static L4::Uart_pl011 _uart(kuart.base_baud);
        _uart.startup(&r);
        set_stdio_uart(&_uart);
      }
    else
      {
        kuart.reg_shift = 2;
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart_flags       |=   L4_kernel_options::F_uart_base
                             | L4_kernel_options::F_uart_baud
                             | L4_kernel_options::F_uart_irq;
        static L4::Uart_16550 _uart(kuart.base_baud, 0, 8);
        setup_16550_mmio_uart(&_uart);
      }
  }

  Boot_modules *modules() { return this; }

  void setup_memory_map()
  {
    // Using multiple of the Mbox_* objects piles up a bit of stack
    // usage, so take care
    Mbox_armmem armmem(_base);
    if (!armmem.call())
      printf("Failed to query VC for RAM info\n");
    Mbox_boardrev br(_base);
    if (!br.call())
      printf("Failed to query VC for board info\n");

    mem_manager->ram->add(Region(armmem.base(), armmem.base() + armmem.size() - 1,
                                 ".ram", Region::Ram));

    // Only available in DT otherwise
    if (br.memory_size_mb() == 8192)
      {
        mem_manager->ram->add(Region(0x040000000, 0x0fbffffff, ".ram", Region::Ram));
        mem_manager->ram->add(Region(0x100000000, 0x1ffffffff, ".ram", Region::Ram));
      }
    if (br.memory_size_mb() == 4096)
      mem_manager->ram->add(Region(0x40000000, 0xfbffffff, ".ram", Region::Ram));
    if (br.memory_size_mb() == 2048)
      mem_manager->ram->add(Region(0x40000000, 0x7fffffff, ".ram", Region::Ram));

    mem_manager->regions->add(Region::n(0x0, 0x1000, ".mpspin",
                                        Region::Arch, 0));

    // The following is just informational
    if (br.new_flag())
      {
        printf("  Raspberry Pi Model %s, Rev 1.%d, %dMB, SoC %s [%x]\n",
               br.type_str(), br.revision(), br.memory_size_mb(), br.processor_str(),
               br.rev_raw());

        printf("  Warranty %s, OTP reading %s, OTP programming %s, Overvoltage %s\n",
               br.warrenty_voided() ? "voided" : "intact",
               br.otp_reading_disallowed() ? "disallowed" : "allowed",
               br.otp_programming_disallowed() ? "disallowed" : "allowed",
               br.overvoltage_disallowed() ? "disallowed" : "allowed");
      }
    else
      {
        printf("  Raspberry Model 1 %s, Rev %d.%d, %dMB\n",
               br.type_str(), br.old_rev_encoded() >> 4, br.old_rev_encoded() & 0xf,
               br.memory_size_mb());
      }
  }

  void reboot()
  {
    enum { Rstc = 0x1c, Wdog = 0x24 };

    L4::Io_register_block_mmio r(_base + 0x00100000);

    l4_uint32_t pw = 0x5a << 24;
    r.write(Wdog, pw | 8);
    r.write(Rstc, (r.read<l4_uint32_t>(Rstc) & ~0x30) | pw | 0x20);

    while (1)
      ;
  }
private:
  l4_addr_t _base;
};
}

REGISTER_PLATFORM(Platform_arm_rpi);
