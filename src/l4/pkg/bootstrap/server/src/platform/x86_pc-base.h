#include "support.h"
#include "startup.h"

#include <l4/drivers/uart_16550.h>
#include <l4/drivers/io_regblock_port.h>

#include <string.h>
#include "base_critical.h"
#include "panic.h"

#include <l4/util/cpu.h>
#include <l4/util/port_io.h>
#include <l4/cxx/static_container>

#include <cassert>
#include <cstdio>

/** VGA console output */

static void vga_init()
{
  /* Reset any scrolling */
  l4util_out32(0xc, 0x3d4);
  l4util_out32(0, 0x3d5);
  l4util_out32(0xd, 0x3d4);
  l4util_out32(0, 0x3d5);
}

static void vga_putchar(unsigned char c)
{
  static int ofs = -1, esc, esc_val, attr = 0x07;
  unsigned char *vidbase = (unsigned char*)0xb8000;

  base_critical_enter();

  if (ofs < 0)
    {
      /* Called for the first time - initialize.  */
      ofs = 80*2*24;
      vga_putchar('\n');
    }

  switch (esc)
    {
    case 1:
      if (c == '[')
	{
	  esc++;
	  goto done;
	}
      esc = 0;
      break;

    case 2:
      if (c >= '0' && c <= '9')
	{
	  esc_val = 10*esc_val + c - '0';
	  goto done;
	}
      if (c == 'm')
	{
	  attr = esc_val ? 0x0f : 0x07;
	  goto done;
	}
      esc = 0;
      break;
    }

  switch (c)
    {
    case '\n':
      memmove(vidbase, vidbase+80*2, 80*2*24);
      memset(vidbase+80*2*24, 0, 80*2);
      /* fall through... */
    case '\r':
      ofs = 0;
      break;

    case '\t':
      ofs = (ofs + 8) & ~7;
      break;

    case '\033':
      esc = 1;
      esc_val = 0;
      break;

    default:
      /* Wrap if we reach the end of a line.  */
      if (ofs >= 80)
	vga_putchar('\n');

      /* Stuff the character into the video buffer. */
	{
	  volatile unsigned char *p = vidbase + 80*2*24 + ofs*2;
	  p[0] = c;
	  p[1] = attr;
	  ofs++;
	}
      break;
    }

done:
  base_critical_leave();
}

/** Poor man's getchar, only returns raw scan code. We don't need to know
 * _which_ key was pressed, we only want to know _if_ a key was pressed. */
static int raw_keyboard_getscancode(void)
{
  unsigned status, scan_code;

  base_critical_enter();

  l4util_cpu_pause();

  /* Wait until a scan code is ready and read it. */
  status = l4util_in8(0x64);
  if ((status & 0x01) == 0)
    {
      base_critical_leave();
      return -1;
    }
  scan_code = l4util_in8(0x60);

  /* Drop mouse events */
  if ((status & 0x20) != 0)
    {
      base_critical_leave();
      return -1;
    }

  base_critical_leave();
  return scan_code;
}


static inline l4_uint32_t
pci_conf_addr(l4_uint32_t bus, l4_uint32_t dev, l4_uint32_t fn, l4_uint32_t reg)
{ return 0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3); }

static l4_uint32_t pci_read(unsigned char bus, l4_uint32_t dev,
                            l4_uint32_t fn, l4_uint32_t reg,
                            unsigned char width)
{
  l4util_out32(pci_conf_addr(bus, dev, fn, reg), 0xcf8);

  switch (width)
    {
    case 8:  return l4util_in8(0xcfc + (reg & 3));
    case 16: return l4util_in16((0xcfc + (reg & 3)) & ~1UL);
    case 32: return l4util_in32(0xcfc);
    }
  return 0;
}

static void pci_write(unsigned char bus, l4_uint32_t dev,
                      l4_uint32_t fn, l4_uint32_t reg,
                      l4_uint32_t val, unsigned char width)
{
  l4util_out32(pci_conf_addr(bus, dev, fn, reg), 0xcf8);

  switch (width)
    {
    case 8:  l4util_out8(val, 0xcfc + (reg & 3)); break;
    case 16: l4util_out16(val, (0xcfc + (reg & 3)) & ~1UL); break;
    case 32: l4util_out32(val, 0xcfc); break;
    }
}


namespace {

struct Resource
{
  enum Type { NO_BAR, IO_BAR, MEM_BAR };
  Type type;
  unsigned long base;
  unsigned long len;
  Resource() : type(NO_BAR) {}
};

enum { NUM_BARS = 6 };

struct Serial_board
{
  enum Flags
  {
    F_bar_per_port = 0x10,
  };

  unsigned short flags;
  unsigned short base_bar;
  unsigned num_ports;
  unsigned base_baud;
  unsigned port_offset;
  unsigned base_offset;
  unsigned reg_shift;

  Resource bars[NUM_BARS];

  unsigned num_mem_bars() const
  {
    unsigned n = 0;
    for (unsigned i = 0; i < NUM_BARS; ++i)
      if (bars[i].type == Resource::MEM_BAR)
        ++n;
    return n;
  }

  unsigned num_io_bars() const
  {
    unsigned n = 0;
    for (unsigned i = 0; i < NUM_BARS; ++i)
      if (bars[i].type == Resource::IO_BAR)
        ++n;
    return n;
  }

  int first_io_bar()
  {
    for (unsigned i = 0; i < NUM_BARS; ++i)
      if (bars[i].type == Resource::IO_BAR)
        return i;
    return -1;
  }

  l4_addr_t get_base(unsigned idx) const
  {
    if (idx >= num_ports)
      idx = 0;

    if (flags & F_bar_per_port)
      return bars[base_bar + idx].base + base_offset;

    return bars[base_bar].base + base_offset + port_offset * idx;
  }

  Resource::Type get_type() const
  { return bars[base_bar].type; }
};

struct Pci_iterator
{
  unsigned bus;
  unsigned dev;
  unsigned func;

  l4_uint32_t vd;

  enum { Max_bus = 20 };

  Pci_iterator() : bus(0), dev(0), func(0) {}
  explicit Pci_iterator(unsigned bus, unsigned dev = 0, unsigned func = 0)
  : bus(bus), dev(dev), func(func) {}

  static Pci_iterator end() { return Pci_iterator(Max_bus, 0, 0); }

  l4_uint32_t pci_read(l4_uint32_t reg, unsigned char width) const
  { return ::pci_read(bus, dev, func, reg, width); }

  void pci_write(l4_uint32_t reg, l4_uint32_t val, unsigned char width) const
  { ::pci_write(bus, dev, func, reg, val, width); }

  void enable_io() const
  {
    unsigned cmd = pci_read(4, 16);
    pci_write(4, cmd | 1, 16);
  }

  void enable_mmio() const
  {
    unsigned cmd = pci_read(4, 16);
    pci_write(4, cmd | 2, 16);
  }

  unsigned vendor() const { return vd & 0xffff; }
  unsigned device() const { return vd >> 16; }
  unsigned vendor_device() const { return vd; }

  unsigned classcode() const { return pci_read(0x0b, 8); }
  unsigned subclass()  const { return pci_read(0x0a, 8); }
  unsigned prog() const { return pci_read(9, 8); }
  unsigned pci_class() const { return pci_read(0x08, 32) >> 8; }

  bool operator == (Pci_iterator const &o) const
  { return bus == o.bus && dev == o.dev && func == o.func; }

  bool operator != (Pci_iterator const &o) const
  { return !operator == (o); }

  Pci_iterator const &operator ++ ()
  {
    for (; bus < Max_bus;)
      {
        if (func == 0)
          {
            unsigned char hdr_type = pci_read(0xe, 8);
            if (hdr_type & 0x80)
              ++func;
            else
              ++dev;
          }
        else if (func < 8)
          ++func;
        else
          {
            func = 0;
            ++dev;
          }

        if (dev >= 32)
          {
            ++bus;
            dev = 0;
            func = 0;
          }

        vd = pci_read(0, 32);
        if (vd == 0xffffffff)
          continue;

        return *this;
      }

    dev = 0;
    func = 0;
    return *this;
  }

};

#if 0
      if (classcode == 0x06 && subclass == 0x04)
        buses++;
#endif



struct Bs_uart : L4::Uart_16550
{
  union Regs
  {
    cxx::Static_container<L4::Io_register_block_port> io;
    cxx::Static_container<L4::Io_register_block_mmio> mem;
  };

  Resource::Type type;
  bool ok;
  Regs uart_regs;

  Bs_uart(Serial_board *board, unsigned port, unsigned long baudrate)
  : L4::Uart_16550(board->base_baud, 0, 0, 0x8 /* out2 */, 0), ok(false)
  {
    type = board->get_type();
    if (type == Resource::IO_BAR)
      {
        unsigned base = board->get_base(port);
        uart_regs.io.construct(base);
        if (!startup(uart_regs.io))
          {
            printf("Could not find or enable UART\n");
            return;
          }
      }
    if (type == Resource::MEM_BAR)
      {
        l4_addr_t base = board->get_base(port);
        uart_regs.mem.construct(base, board->reg_shift);
        if (!startup(uart_regs.mem))
          {
            printf("Could not find or enable UART\n");
            return;
          }
      }

    if (!change_mode(L4::Uart_16550::MODE_8N1, baudrate))
      {
        printf("Could not find or enable UART\n");
        return;
      }

    ok = true;
  }

};

class Dual_uart : public L4::Uart
{
private:
  L4::Uart *_u1, *_u2;

public:
  Dual_uart(L4::Uart *u1)
  : _u1(u1), _u2(0)
  {}

  void set_uart2(L4::Uart *u2)
  {
    _u2 = u2;
  }

  bool startup(L4::Io_register_block const *)
  {
    return true;
  }

  void shutdown()
  {
    _u1->shutdown();
    if (_u2)
      _u2->shutdown();
  }

  ~Dual_uart() {}
#if 0
  bool enable_rx_irq(bool e)
  {
    bool r1 = _u1->enable_rx_irq(e);
    bool r2 = _u2 ? _u2->enable_rx_irq(e) : false;
    return r1 && r2;
  }

  bool enable_tx_irq(bool e)
  {
    bool r1 = _u1->enable_tx_irq(e);
    bool r2 = _u2 ? _u2->enable_tx_irq(e) : false;
    return r1 && r2;
  }
#endif

  bool change_mode(Transfer_mode m, Baud_rate r)
  {
    bool r1 = _u1->change_mode(m, r);
    bool r2 = _u2 ? _u2->change_mode(m, r) : false;
    return r1 && r2;
  }

  int char_avail() const
  {
    return _u1->char_avail() || (_u2 && _u2->char_avail());
  }

  int get_char(bool blocking) const
  {
    int c;
    do
      {
        c = _u1->get_char(false);
        if (c == -1 && _u2)
          c = _u2->get_char(false);
      }
    while (blocking && c == -1);
    return c;
  }

  int write(char const *s, unsigned long count) const
  {
    int r = _u1->write(s, count);
    if (_u2)
      _u2->write(s, count);
    return r;
  }

};


}

struct Pci_com_drv
{
  virtual bool setup(Pci_iterator const &i, Serial_board *board) const = 0;
  virtual ~Pci_com_drv() = 0;

  static void read_bars(Pci_iterator const &i, Serial_board *board);

};

inline Pci_com_drv::~Pci_com_drv() {}

void
Pci_com_drv::read_bars(Pci_iterator const &dev, Serial_board *board)
{
  for (int bar = 0; bar < NUM_BARS; ++bar)
    {
      int a = 0x10 + bar * 4;

      unsigned v = dev.pci_read(a, 32);
      dev.pci_write(a, ~0U, 32);
      unsigned x = dev.pci_read(a, 32);
      dev.pci_write(a, v, 32);

      if (!v)
        continue;

      int s;
      for (s = 2; s < 32; ++s)
        if ((x >> s) & 1)
          break;

      board->bars[bar].base = v & ~3UL;
      board->bars[bar].len = 1 << s;
      board->bars[bar].type = (v & 1) ? Resource::IO_BAR : Resource::MEM_BAR;

      if (0)
        printf("BAR%d: %04x (sz=%d)\n", bar, v & ~3, 1 << s);
    }
}


struct Pci_com_drv_default : Pci_com_drv
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    read_bars(dev, board);
    int num_iobars = board->num_io_bars();

    if (num_iobars != 1)
      return false;

    //int num_membars = board->num_mem_bars();
    //if (num_membars > 1)
    //  return false;

    int first_port = board->first_io_bar();

    board->port_offset = 8;
    board->base_baud = L4::Uart_16550::Base_rate_x86;
    board->base_bar = first_port;
    board->num_ports = board->bars[first_port].len / board->port_offset;
    board->flags = 0;
    printf("   detected serial IO card: bar=%d ports=%d\n",
           first_port, board->num_ports);
    dev.enable_io();
    return true;
  }
};

struct Pci_com_drv_fallback : Pci_com_drv_default
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    // The default drivers only takes 7:80 typed devices
    if (dev.classcode() != 7)
      return false;

    if (dev.subclass() != 0x80)
      return false;

    return Pci_com_drv_default::setup(dev, board);
  }
};




struct Pci_com_drv_oxsemi : Pci_com_drv
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    read_bars(dev, board);
    board->flags = 0;
    board->num_ports = 0;

    switch (dev.device())
      {
      case 0xc158:
        board->num_ports = 2;
        board->base_baud = 4000000;
        board->port_offset = 0x200;
        board->base_bar = 0;
        board->base_offset = 0x1000;
        board->reg_shift = 0;
        break;
      default:
        break;
      }

    if (!board->num_ports)
      return false;

    printf("  found oxsemi PCI controller.\n");
    dev.enable_mmio();
    return true;
  }
};

struct Pci_com_moschip : public Pci_com_drv
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    read_bars(dev, board);

    int first_port = board->first_io_bar();

    board->port_offset = 8;
    board->base_baud = L4::Uart_16550::Base_rate_x86;
    board->base_bar = first_port;
    board->num_ports = board->bars[first_port].len / board->port_offset;
    board->flags = 0;
    printf("   detected serial IO card: bar=%d ports=%d\n",
           first_port, board->num_ports);
    dev.enable_io();
    return true;
  }
};

struct Pci_com_agestar : public Pci_com_drv
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    read_bars(dev, board);

    board->port_offset = 8;
    board->base_baud = L4::Uart_16550::Base_rate_x86;
    board->base_bar = board->first_io_bar();;
    board->num_ports = 2;
    board->flags = 0;
    printf("   detected serial IO card: bar=%d ports=%d\n",
           board->base_bar, board->num_ports);
    dev.enable_io();
    return true;
  }
};

struct Pci_com_wch_chip : public Pci_com_drv
{
  bool setup(Pci_iterator const &dev, Serial_board *board) const
  {
    read_bars(dev, board);

    board->port_offset = 8;
    board->base_baud = L4::Uart_16550::Base_rate_x86;
    board->base_bar = board->first_io_bar();
    board->num_ports = 2;
    board->base_offset = 0xc0;
    board->flags = 0;
    printf("   detected serial IO card: bar=%d ports=%d\n",
           board->base_bar, board->num_ports);
    dev.enable_io();
    return true;
  }
};

static Pci_com_drv_fallback _fallback_pci_com;
static Pci_com_drv_default _default_pci_com;
static Pci_com_drv_oxsemi _oxsemi_pci_com;
static Pci_com_moschip _moschip;
static Pci_com_agestar _agestar;
static Pci_com_wch_chip _wch_chip;

#define PCI_DEVICE_ID(vendor, device) \
  (((unsigned)(device) << 16) | (unsigned)(vendor & 0xffff)), 0xffffffffU
#define PCI_ANY_DEVICE 0, 0

struct Pci_com_dev
{
  unsigned vendor_device;
  unsigned mask;
  Pci_com_drv *driver;
};

Pci_com_dev _devs[] = {
  { PCI_DEVICE_ID(0x1415, 0xc158), &_oxsemi_pci_com },
  { PCI_DEVICE_ID(0x9710, 0x9835), &_moschip },
  { PCI_DEVICE_ID(0x9710, 0x9865), &_moschip },
  { PCI_DEVICE_ID(0x9710, 0x9922), &_moschip },
  { PCI_DEVICE_ID(0x5372, 0x6872), &_agestar },
  { PCI_DEVICE_ID(0x1c00, 0x3253), &_wch_chip }, // dual port card
  { PCI_DEVICE_ID(0x8086, 0x8c3d), &_default_pci_com },
  { PCI_ANY_DEVICE, &_fallback_pci_com },
};

enum { Num_known_devs = sizeof(_devs) / sizeof(_devs[0]) };


static Pci_iterator
_search_pci_serial_devs(Pci_iterator const &begin, Pci_iterator const &end,
                        Serial_board *board, bool print)
{
  for (Pci_iterator i = begin; i != end; ++i)
    {
      if (print)
        printf("%02x:%02x.%1x Class %02x.%02x Prog %02x: %04x:%04x\n",
               i.bus, i.dev, i.func, i.classcode(), i.subclass(), i.prog(),
               i.vendor(), i.device());

      for (unsigned devs = 0; devs < Num_known_devs; ++devs)
        if (   (i.vendor_device() & _devs[devs].mask) == _devs[devs].vendor_device
            && _devs[devs].driver->setup(i, board))
          return i;
    }

  return end;
}

static unsigned long
search_pci_serial_devs(int card_idx, Serial_board *board)
{
  Pci_iterator end = Pci_iterator::end();
  Pci_iterator i;
  for (int card = 0; i != end; ++i, ++card) {
    i = _search_pci_serial_devs(i, end, board, false);
    if (card == card_idx)
      return 1;
  }

  return 0;
}

static void
scan_pci_uarts(Dual_uart *du, unsigned long baudrate)
{
  Serial_board board;
  Pci_iterator end = Pci_iterator::end();
  // classes should be 7:0
  Pci_iterator i;
  for (int card = 0; i != end; ++i, ++card)
    {
      i = _search_pci_serial_devs(i, end, &board, true);

      if (i == end)
        break;

      for (unsigned p = 0; p < board.num_ports; ++p)
        {
          printf("probed PCI UART %02x:%02x.%1x\n",
                 i.bus, i.dev, i.func);
          Bs_uart u(&board, p, baudrate);
          du->set_uart2(&u);
          printf("probed PCI UART %02x:%02x.%1x\n"
                 "to use this port set: -comport=pci:%d:%d\n",
                 i.bus, i.dev, i.func, card, p);
          du->set_uart2(0);
        }
    }
  printf("done scanning PCI uarts\n");
}

class Uart_vga : public L4::Uart
{
public:
  Uart_vga()
  { }

  bool startup(L4::Io_register_block const *)
  {
    vga_init();
    return true;
  }

  ~Uart_vga() {}
  void shutdown() {}
  bool enable_rx_irq(bool) { return false; }
  bool enable_tx_irq(bool) { return false; }
  bool change_mode(Transfer_mode, Baud_rate) { return true; }
  int get_char(bool blocking) const
  {
    int c;
    do
      c = raw_keyboard_getscancode();
    while (blocking && c == -1);
    return c;
  }

  int char_avail() const
  {
    return raw_keyboard_getscancode() != -1;
  }

  int write(char const *s, unsigned long count) const
  {
    unsigned long c = count;
    while (c)
      {
        if (*s == 10)
          vga_putchar(13);
        vga_putchar(*s++);
        --c;
      }
    return count;
  }
};


static void
legacy_uart(unsigned com_port_or_base, int &com_irq, Serial_board *board)
{
  switch (com_port_or_base)
    {
    case 1: com_port_or_base = 0x3f8;
            if (com_irq == -1)
              com_irq = 4;
            break;
    case 2: com_port_or_base = 0x2f8;
            if (com_irq == -1)
              com_irq = 3;
            break;
    case 3: com_port_or_base = 0x3e8; break;
    case 4: com_port_or_base = 0x2e8; break;
    }

  board->num_ports = 1;
  board->bars[0].type = Resource::IO_BAR;
  board->bars[0].base = com_port_or_base;
  board->base_bar = 0;
  board->flags = 0;
  board->base_offset = 0;
  board->port_offset = 8;
  board->base_baud = L4::Uart_16550::Base_rate_x86;
  board->reg_shift = 0;
}


class Platform_x86 : public Platform_base
{
public:
#ifdef ARCH_amd64
  boot32_info_t const *boot32_info;
#endif
  bool probe() { return true; }

  int init_uart(Serial_board *board, int port, int com_irq, Dual_uart *du)
  {
    base_critical_enter();
    unsigned baudrate = 115200;

    static cxx::Static_container<Bs_uart> uart;
    uart.construct(board, port, baudrate);

    if (!uart->ok)
      {
        base_critical_leave();
        return 1;
      }

    if (uart->type == Resource::IO_BAR)
      {
        unsigned base = board->get_base(port);
        kuart.access_type  = L4_kernel_options::Uart_type_ioport;
        kuart.base_address = base;
        printf("UART IO: @%x\n", base);
      }
    if (uart->type == Resource::MEM_BAR)
      {
        l4_addr_t base = board->get_base(port);
        kuart.access_type  = L4_kernel_options::Uart_type_mmio;
        kuart.base_address = base;
        printf("UART MMIO: @%p\n", (void*)base);
      }

    kuart.base_baud = board->base_baud;
    kuart.baud = baudrate;
    kuart.reg_shift = board->reg_shift;

    du->set_uart2(uart);

    kuart.irqno        = com_irq;
    kuart_flags       |=   L4_kernel_options::F_uart_base
                         | L4_kernel_options::F_uart_baud;
    if (com_irq != -1)
      kuart_flags |= L4_kernel_options::F_uart_irq;

    base_critical_leave();
    return 0;
  }

  static unsigned cpuid_features()
  {
    unsigned ver, brand, ext_feat, feat;

    asm("cpuid"
	: "=a" (ver), "=b" (brand), "=c" (ext_feat), "=d" (feat)
	: "a" (1));

    return feat;
  }

  /* To use esp. library code in bootstrap, we enable the FPU
   * so that FPU usage by those libs is possible */
  void init()
  {
    unsigned long tmp;

    /* Enable CR4_OSXSAVE if SSE2 is available */
    if (cpuid_features() & (1 << 26))
      asm volatile("mov %%cr4, %0      \n\t"
                   "or  $(1 << 9), %0  \n\t"
                   "mov %0, %%cr4      \n\t"
                   : "=r" (tmp));
  }

  void boot_kernel(unsigned long entry)
  {
    unsigned long tmp;

    /* Disable CR4_OSXSAVE if SSE2 is available */
    if (cpuid_features() & (1 << 26))
      asm volatile("mov %%cr4, %0      \n\t"
                   "and $~(1 << 9), %0 \n\t"
                   "mov %0, %%cr4      \n\t"
                   : "=r" (tmp));


    typedef void (*func)(void);
    ((func)entry)();
    exit(-100);
  }

  void setup_uart(char const *cmdline)
  {
    const char *s;
    int comport = -1;
    int comirq = -1;
    int pci = 0;
    static Uart_vga _vga;
    static Dual_uart du(&_vga);
    set_stdio_uart(&du);

    if ((s = check_arg(cmdline, "-comirq")))
      {
        s += 8;
        comirq = strtoul(s, 0, 0);
      }

    if ((s = check_arg(cmdline, "-comport")))
      {
        s += 9;
        if ((pci = !strncmp(s, "pci:", 4)))
          s += 4;

        if (pci && !strncmp(s, "probe", 5))
          {
            scan_pci_uarts(&du, 115200);
            reboot();
          }

        char *ep;
        comport = strtoul(s, &ep, 0);
        if (pci && *ep == ':')
          {
            pci = comport + 1;
            comport = strtoul(ep + 1, 0, 0);
          }
      }

    if (!check_arg(cmdline, "-noserial"))
      {
        Serial_board board;
        if (pci)
          {
            if (!search_pci_serial_devs(pci - 1, &board))
              {
                legacy_uart(1, comirq, &board);
                comport = 0;
              }
          }
        else if (comport == -1)
          {
            legacy_uart(1, comirq, &board);
            comport = 0;
          }
        else
          {
            legacy_uart(comport, comirq, &board);
            comport = 0;
          }

        if (init_uart(&board, comport, comirq, &du))
          {
            kuart_flags |= L4_kernel_options::F_noserial;
            printf("UART init failed\n");
          }
      }
    else
      kuart_flags |= L4_kernel_options::F_noserial;
  }
};
