/**
 * \file	x86emu/lib/int10/int10.c(c)
 * \brief	Call VESA BIOS functions using the real mode interface
 *
 * \date	2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *              Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *              Alexander Warg <warg@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/sys/types.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
#include <l4/x86emu/x86emu.h>
#include <l4/re/env>
#include <l4/re/mem_alloc>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>
#include <l4/re/namespace>
#include <l4/re/dataspace>
#include <l4/util/mb_info.h>
#include <l4/sys/err.h>
#include <l4/io/io.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_pci.h>
#include <l4/cxx/hlist>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <l4/x86emu/int10.h>

#include <l4/libedid/edid.h>

namespace {
struct ioport_res : public cxx::H_list_item
{
  ioport_res(l4_uint16_t start, l4_uint16_t end)
  : start(start), end(end)
  {
    _list.push_front(this);
  }

  unsigned short start, end;
  bool warned;
  typedef cxx::H_list_bss<ioport_res> List;
  static List _list;

  static bool port_is_avail(unsigned port, unsigned size);
};

ioport_res::List ioport_res::_list;

bool
ioport_res::port_is_avail(unsigned port, unsigned size)
{
  for (List::Const_iterator i = _list.begin(); i != _list.end(); ++i)
    if (i->start <= port && port + size - 1 <= i->end)
      return true;
  return false;
}
}

static void
port_request_cb(l4vbus_resource_t const *r)
{
  new ioport_res(r->start, r->end);
}

enum {
  Dbg_io  = 0,
  Dbg_mem = 0,
  Dbg_pci = 0,
};


static l4_addr_t   v_page[1024*1024/(L4_PAGESIZE)];
static l4_addr_t v_area;
static l4_umword_t initialized;
static L4::Cap<void> vbus;
static l4vbus_device_handle_t root_bridge;

static void
warn(u32 addr, const char *func)
{
  printf("\033[31mWARNING: Function %s access %08lx\033[m\n", func,
         (unsigned long)addr);
}

#define PCI_SLOT(devfn)         (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)         ((devfn) & 0x07)

static int do_pcicfg(unsigned long addr, u32 *val32, int size, int out)
{
  static l4_uint32_t config_reg;
  /* check for PCI addr and data ports */
  if (addr < 0xcf8 || addr >= 0xd00)
    return 0;

  if (addr == 0xcf8)
    {
      config_reg = *val32;
      return 1;
    }

  if (addr >= 0xcfc)
    {
      l4_uint32_t reg;
      l4_uint32_t v;
      unsigned df = (config_reg >> 8) & 0xff;
      unsigned bus = (config_reg >> 16) & 0xff;

      df = (PCI_SLOT(df) << 16) | PCI_FUNC(df);

      reg = (addr & 3) | (config_reg & 0xfc) | ((config_reg >> 16) & 0xf00);
      if (Dbg_pci)
        printf("CFG ACCESS(%d-%d): %02x:%03x.%02x reg=%x val=%p (*val=%lx)\n",
               out, size, bus, (df >> 16) & 0xffff, df & 0xffff,
               reg, val32, (unsigned long)*val32);

      if (out)
        l4vbus_pci_cfg_write(vbus.cap(), root_bridge, bus, df, reg,
                             *val32, size);
      else
        {
          l4vbus_pci_cfg_read(vbus.cap(), root_bridge, bus, df, reg, &v, size);
	  *val32 = v;
        }
    }
  else
    printf("Uups: %lx\n", addr);

  if (Dbg_pci)
    printf("access to PCI %lx %s %lx\n", addr, out ? "WRITE" : "READ",
	   (unsigned long)*val32);

  return 1;
}

static void
port_message(unsigned port, unsigned size, u32 value,
             const char *insn_tag, const char *add_tag = "")
{
  static char const *const ws[4] =  { "b", "w", "X", "l" };
  printf("%04x:%04x %s%s %x -> %lx%s\n", M.x86.R_CS, M.x86.R_IP,
         insn_tag, ws[size - 1], port, (unsigned long)value, add_tag);
}

template< typename W >
static W X86API
port_in(X86EMU_pioAddr addr)
{
  u32 r;
  if (do_pcicfg(addr, &r, sizeof(W) * 8, 0))
    return r;

  if (!ioport_res::port_is_avail(addr, sizeof(W)))
    {
      port_message(addr, sizeof(W), r, "in", " ignored");
      return 0;
    }

  switch (sizeof(W))
    {
    case 1: asm volatile ("in %w1, %b0" : "=a" (r) : "d" (addr)); break;
    case 2: asm volatile ("in %w1, %w0" : "=a" (r) : "d" (addr)); break;
    case 4: asm volatile ("in %w1, %0" : "=a" (r) : "d" (addr)); break;
    default: r = 0;
    }

  if (Dbg_io)
    port_message(addr, sizeof(W), r, "in");

  return r;
}


template< typename W >
static void X86API
port_out(X86EMU_pioAddr addr, W val)
{
  u32 r = val;
  if (do_pcicfg(addr, &r, sizeof(W) * 8, 1))
    return;

  if (!ioport_res::port_is_avail(addr, sizeof(W)))
    {
      port_message(addr, sizeof(W), r, "out", " ignored");
      return;
    }

  if (Dbg_io)
    port_message(addr, sizeof(W), r, "out");


  switch (sizeof(W))
    {
    case 1: asm volatile ("out %b0, %w1" : : "a" (r), "d" (addr)); break;
    case 2: asm volatile ("out %w0, %w1" : : "a" (r), "d" (addr)); break;
    case 4: asm volatile ("out %0, %w1"  : : "a" (r), "d" (addr)); break;
    default: break;
    }
}

template< typename W >
static W X86API
mem_read(u32 addr)
{
  if (addr > (1 << 20))
    warn(addr, __FUNCTION__);

  if (Dbg_mem)
    printf("read(%d) %08x => ", (int)sizeof(W), (unsigned)addr);

  W const *a = (W const *)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE));

  if (Dbg_mem)
    printf("%0*x\n", (int)sizeof(W)*2, (unsigned)*a);

  return *a;
}

template< typename W >
static void X86API
mem_write(u32 addr, W val)
{
  if (addr > (1 << 20))
    warn(addr, __FUNCTION__);

  if (Dbg_mem)
    printf("write(%d) %0*x => %08x\n", (int)sizeof(W), (int)sizeof(W)*2, (unsigned)val, (unsigned)addr);

  W *a = (W *)(v_page[addr/L4_PAGESIZE] + (addr % L4_PAGESIZE));
  *a = val;
}


X86EMU_pioFuncs my_pioFuncs =
{
  port_in<u8>,  port_in<u16>,  port_in<u32>,
  port_out<u8>, port_out<u16>, port_out<u32>
};

X86EMU_memFuncs my_memFuncs =
{
  mem_read<u8>,  mem_read<u16>,  mem_read<u32>,
  mem_write<u8>, mem_write<u16>, mem_write<u32>
};

void
printk(const char *format, ...)
{
  va_list list;
  va_start(list, format);
  vprintf(format, list);
  va_end(list);
}


/* VESA access functions start here */

enum Function
{
  VBE_get_info      = 0x4f00,
  VBE_get_mode_info = 0x4f01,
  VBE_set_mode      = 0x4f02,
  VBE_set_base_addr = 0x4f07,
  VBE_power         = 0x4f10,
  VBE_ddc           = 0x4f15,
};

static int
x86emu_int10_init(void)
{
  int error;
  l4_addr_t addr;
  l4_uint32_t idx;
  L4::Cap<L4Re::Dataspace> ds;

  if (initialized)
    return 0;

  int err;
  vbus = L4Re::Env::env()->get_cap<void>("vbus");
  if (!vbus.is_valid())
    {
      printf("no 'vbus' found in namespace\n");
      return -L4_ENOENT;
    }


  err = l4vbus_get_device_by_hid(vbus.cap(), 0, &root_bridge, "PNP0A03", 0, 0);
  if (err < 0)
    {
      printf("No PCI root bridge found\n");
      L4Re::Util::cap_alloc.free(vbus);
      return -L4_ENOENT;
    }


  X86EMU_setupPioFuncs(&my_pioFuncs);
  X86EMU_setupMemFuncs(&my_memFuncs);
  M.x86.debug = 0 /*| DEBUG_DECODE_F*/;

  l4io_request_all_ioports(port_request_cb);

  /* Reserve region for physical memory 0x00000...0xfffff. Make sure that we
   * can unmap it using one single l4_fpage_unmap (alignment). */
  v_page[0] = 0;
  if ((error = L4Re::Env::env()->rm()->reserve_area(&v_page[0], 1<<20,
                                                    L4Re::Rm::Search_addr, 20)))
    {
      printf("Error %d reserving area for x86emu\n", error);
      return error;
    }
  v_area = v_page[0];

  /* Map physical page 0x00000 */
  if (l4io_request_iomem_region(0, v_page[0], L4_PAGESIZE, L4IO_MEM_CACHED | L4IO_MEM_USE_RESERVED_AREA) < 0)
    {
      printf("Error %d allocating physical page 0 for x86emu\n", error);
      return error;
    }

  ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!ds.is_valid())
    return -L4_ENOMEM;

  if ((error = L4Re::Env::env()->mem_alloc()->alloc(L4_PAGESIZE, ds, 0)))
    {
      printf("Error %d allocating page for x86emu\n", error);
      return error;
    }

  /* Map dummy as physical page 0x01000 */
  v_page[1] = v_page[0] + L4_PAGESIZE;
  if ((error = L4Re::Env::env()->rm()->attach((void**)&v_page[1], L4_PAGESIZE,
                                              L4Re::Rm::In_area, ds,
                                              0, L4_PAGESHIFT)))
    {
      printf("Error %d attaching page for x86emu\n", error);
      return error;
    }

  for (idx=0x09F000/L4_PAGESIZE, addr=0x09F000;
                                 addr<0x100000;
       idx++,			 addr+=L4_PAGESIZE)
    v_page[idx] = v_page[0] + addr;

  if ((error = l4io_request_iomem_region(0x09F000, v_page[0x9f],
                                         0x100000 - 0x09F000,
                                         L4IO_MEM_CACHED |
                                           L4IO_MEM_USE_RESERVED_AREA)) < 0)
    {
      printf("BIOS area mapping failed: %x\n", error);
      return error;
    }

  /* int 10 ; ret */
  *(l4_uint8_t*)(v_page[1]+0) = 0xcd;
  *(l4_uint8_t*)(v_page[1]+1) = 0x10;
  *(l4_uint8_t*)(v_page[1]+2) = 0xf4;

  initialized = 1;

  return 0;
}

static void setup_int10_entry(l4_uint16_t function)
{
  M.x86.R_EAX  = function;
  M.x86.R_IP   = 0;		/* address of "int10; hlt" */
  M.x86.R_SP   = L4_PAGESIZE;	/* SS:SP pointer to stack */
  M.x86.R_CS   =
  M.x86.R_DS   =
  M.x86.R_ES   =
  M.x86.R_SS   = L4_PAGESIZE >> 4;
}

static bool exec_int10()
{
  X86EMU_exec();
  return M.x86.R_AX == 0x004F;
}

int
x86emu_int10_done(void)
{
  int ret;
  unsigned i = 0;

  if (!initialized)
    return 0;

  for (; i < sizeof(v_page) / sizeof(v_page[0]); ++i)
    if (v_page[i])
      L4Re::Env::env()->rm()->detach((void *)v_page[i], 0);

  /* Free the area */
  ret = L4Re::Env::env()->rm()->free_area(v_area);

  initialized = 0;

  return ret;
}

static l4_addr_t far_to_addr(l4_uint32_t farp)
{
  l4_addr_t p = (farp & 0x0FFFF) + ((farp >> 12) & 0xFFFF0);
  return (l4_addr_t)(v_page[p / L4_PAGESIZE] + (p % L4_PAGESIZE));
}

static void dump_mode(int mode, l4util_mb_vbe_mode_t *m)
{
  char s[16];

  if (!m)
    {
      printf("Mode: 0x%x: No information available\n", mode);
      return;
    }

  snprintf(s, sizeof(s), "%dx%d@%d",
           m->x_resolution, m->y_resolution, m->bits_per_pixel);
  s[sizeof(s) - 1] = 0;

  printf("Mode: 0x%x %13s, RGB: %d(%d):%d(%d):%d(%d) mode: %x\n",
         mode, s,
         m->red_field_position, m->red_mask_size,
         m->green_field_position, m->green_mask_size,
         m->blue_field_position, m->blue_mask_size,
         m->mode_attributes);
}

static
l4util_mb_vbe_mode_t *get_mode_info(int mode)
{
  setup_int10_entry(VBE_get_mode_info);
  M.x86.R_ECX  = mode;		/* VESA mode */
  M.x86.R_EDI  = 0x800;		/* ES:DI pointer to at least 256 bytes */
  if (!exec_int10())
    return 0;

  l4util_mb_vbe_mode_t *m = (l4util_mb_vbe_mode_t*)(v_page[1] + 0x800);
  if ((m->mode_attributes & 0x91) == 0x91)
    return m;

  return 0;
}

static int
x86emu_int10_read_ddc(unsigned *xres, unsigned *yres)
{
  int error;

  if ((error = x86emu_int10_init()))
    return error;

  printf("Trying to read DDC info\n");

  // test whether DDC function is supported
  setup_int10_entry(VBE_ddc);
  M.x86.R_EBX = 0;
  M.x86.R_ECX = 0;
  if (!exec_int10())
    {
      printf("DDC function not supported\n");
      return -L4_ENODEV;
    }

  printf("DDC level supported: 0x%x, screen blanked: %d\n",
         M.x86.R_EBX & 0x3, M.x86.R_EBX & 0x4);
  printf("Read EDID data\n");

  // read EDID data block
  setup_int10_entry(VBE_ddc);
  M.x86.R_EBX = 1; // read EDID
  M.x86.R_ECX = 0; // controller number, 0 = primary controller
  M.x86.R_EDX = 0; // EDID block number
  M.x86.R_EDI = 0x100; // ES.DI: pointer to 128 byte block where EDID block shall be returned
  if (!exec_int10())
    {
      printf("EDID call failed 0x%x\n", M.x86.R_AX);
      return -L4_EINVAL;
    }

  const unsigned char *edid = (const unsigned char *)(v_page[1] + 0x100);

  printf("EDID Version %u.%u\n", libedid_version(edid), libedid_revision(edid));
  printf("   %u extension blocks\n", libedid_num_ext_blocks(edid));

  // dump EDID block (debugging)
  if (1)
    libedid_dump(edid);

  // read manufacturer ID
  unsigned char id[4];
  libedid_pnp_id(edid, id);
  printf("PNP ID=%s\n", id);

  libedid_prefered_resolution(edid, xres, yres);

  printf("Prefered mode: %ux%u\n", *xres, *yres);

  return 0;
}

static
bool is_better_than(l4util_mb_vbe_mode_t *best, l4util_mb_vbe_mode_t *mode)
{
  if (best->x_resolution == 0)
    return true;

  // the bigger the better
  if (   mode->x_resolution > best->x_resolution
      || mode->y_resolution > best->y_resolution)
    return true;

  if (   mode->x_resolution < best->x_resolution
      || mode->y_resolution < best->y_resolution)
    return false;

  // prefer 16bit
  return mode->bits_per_pixel == 16;
}

int
x86emu_int10_set_vbemode(int mode, l4util_mb_vbe_ctrl_t *ctrl_info,
			 l4util_mb_vbe_mode_t *mode_info)
{
  int error;

  if ((error = x86emu_int10_init()))
    return error;

  printf("Trying execution of ``set VBE mode'' using x86emu\n");

  /* Get VESA BIOS controller information. */
  setup_int10_entry(VBE_get_info);
  M.x86.R_EDI  = 0x100;		/* ES:DI pointer to at least 512 bytes */
  if (!exec_int10())
    {
      printf("VBE BIOS not present.\n");
      return -L4_EINVAL;
    }

  *ctrl_info = *(l4util_mb_vbe_ctrl_t*)(v_page[1] + 0x100);
  const char *oem_string = (const char *)far_to_addr(ctrl_info->oem_string);
  printf("Found VESA BIOS version %d.%d\n"
         "OEM %s\n",
         (int)(ctrl_info->version >> 8),
         (int)(ctrl_info->version &  0xFF),
         ctrl_info->oem_string ? oem_string : "[unknown]");
  if (ctrl_info->version < 0x0200)
    {
      printf("VESA BIOS 2.0 or later required.\n");
      return -L4_EINVAL;
    }

  if (mode == ~0)
    {
      // mode == ~0 means to look for the 'best' mode available, we'll look
      // for the highest 16bit mode
      int max_val = 0;
      int res;
      unsigned pref_x = 0, pref_y = 0;

      printf("Scanning for 'best' possible mode:\n");
      l4_uint16_t *mode_list = (l4_uint16_t *)far_to_addr(ctrl_info->video_mode);

      if ((res = x86emu_int10_read_ddc(&pref_x, &pref_y)) < 0)
        printf("EDID not available (%d), finding best possible mode ...\n", res);

      l4util_mb_vbe_mode_t best_mode;
      best_mode.x_resolution = 0;

      for (; *mode_list != 0xffff; ++mode_list)
        {
          l4util_mb_vbe_mode_t *m = get_mode_info(*mode_list);
          if (!m)
            continue;

          dump_mode(*mode_list, m);

          if (pref_x > 0)
            {
              if (   m->x_resolution <= pref_x && m->y_resolution <= pref_y
                  && is_better_than(&best_mode, m))
                {
                  best_mode = *m;
                  mode = *mode_list;
                }
            }
          else if (m->x_resolution > max_val && m->bits_per_pixel == 16)
            {
              max_val = m->x_resolution;
              mode = *mode_list;
              best_mode = *m;
            }
        }
      if (mode == ~0)
        {
          printf("Could not find suitable mode\n");
          return -L4_EINVAL;
        }
      printf("Choosen mode:\n");
      dump_mode(mode, get_mode_info(mode));
      printf("To force a specific setting use a '-m <mode>' option.\n");
      *mode_info = best_mode;
    }
  else
    {
      l4util_mb_vbe_mode_t *mi = get_mode_info(mode);
      if (!mi)
        {
          printf("Mode %x not supported\n", mode);
          printf("List of supported graphics modes:\n");
          l4_uint16_t *mode_list = (l4_uint16_t *)far_to_addr(ctrl_info->video_mode);
          for (; *mode_list != 0xffff; ++mode_list)
            dump_mode(*mode_list, get_mode_info(*mode_list));

          return -L4_EINVAL;
        }
      *mode_info = *mi;
    }

  /* Switch mode. */
  setup_int10_entry(VBE_set_mode);
  M.x86.R_EBX  = mode & 0xf7ff;	/* VESA mode; use current refresh rate */
  M.x86.R_EBX |= (1<<14);	/* use flat buffer model */
  if (0)
    M.x86.R_EBX |= (1<<15);	/* no screen clearing */
  X86EMU_exec();
  // error check missing ?
  printf("VBE mode 0x%x successfully set.\n", mode);
  return mode;
}

int
x86emu_int10_pan(unsigned *x, unsigned *y)
{
  int error;

  if ((error = x86emu_int10_init()))
    return error;

  printf("Trying execution of ``set display start'' using x86emu\n");

  /* Get VESA BIOS controller information. */
  setup_int10_entry(VBE_set_base_addr);
  M.x86.R_BL   = 0x00;          /* set display start */
  M.x86.R_CX   = *x;            /* first displayed pixel in scanline */
  M.x86.R_DX   = *y;            /* first displayed scanline */
  if (!exec_int10())
    {
      printf("Panning to %d,%d not supported\n", *x, *y);
      return -L4_EINVAL;
    }

  printf("Successful.\n");
  return 0;
}
