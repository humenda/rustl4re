/*
 * AMBA CLCD PL110 driver
 */

#include <l4/drivers/lcd.h>
#include <l4/drivers/amba.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <l4/re/c/dataspace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/io/io.h>

#include "lcd-amba.h"

int config_do_bgr;
int config_request_xga;
int config_use_565;

enum pl11x_type
{
  UNINITIALIZED, PL110, PL111, PL_UNKNOWN
};

static enum pl11x_type type;
static void *fb_vaddr;
static l4_addr_t fb_paddr;
static int is_qemu;
static int use_xga;


static void config(const char *c)
{
  // this driver scans for the following:
  //   1024: XGA mode, VGA mode otherwise
  //   BGR:  do bgr mode instead of rgb
  //   565: mode (if PL111 available)
  if (!c)
    return;

  config_request_xga = strcasestr(c, "1024") != NULL;
  config_do_bgr      = strcasestr(c, "bgr") != NULL;
  config_use_565     = strcasestr(c, "565") != NULL;
}



static void set_colors(l4re_video_view_info_t *vinfo,
                       int wr, int wg, int wb,
                       int sr, int sg, int sb)
{
  vinfo->pixel_info.r.shift = sr;
  vinfo->pixel_info.r.size  = wr;
  vinfo->pixel_info.g.shift = sg;
  vinfo->pixel_info.g.size  = wg;
  vinfo->pixel_info.b.shift = sb;
  vinfo->pixel_info.b.size  = wb;

  printf("Color mode: %d:%d:%d  %d:%d:%d\n", sr, sg, sb, wr, wg, wb);
}

static int get_fbinfo(l4re_video_view_info_t *vinfo)
{

  vinfo->width                      = use_xga ? 1024 : 640;
  vinfo->height                     = use_xga ? 768 : 480;
  vinfo->pixel_info.bytes_per_pixel = 2;
  vinfo->bytes_per_line             = 2 * vinfo->width;

  if ((config_use_565 && type == PL111) || (is_qemu && type == PL110))
    {
      if (config_do_bgr)
        set_colors(vinfo, 5, 6, 5, 11, 5, 0);
      else
        set_colors(vinfo, 5, 6, 5, 0, 5, 11);
    }
  else
    {
      if (config_do_bgr)
        set_colors(vinfo, 5, 5, 5, 10, 5, 0);
      else
        set_colors(vinfo, 5, 5, 5, 0, 5, 10);
    }

  return 0;
}

static int probe(const char *c)
{
  config(c);

  return !l4io_lookup_device("AMBA PL110", NULL, 0, 0);
}

static unsigned int fbmem_size(void)
{ return use_xga ? (1024 * 768 * 2) : (480 * 640 * 2); }

unsigned long amba_pl110_lcd_control_virt_base;
unsigned long amba_pl110_sys_base_virt;

static void setup_type(void)
{
  uint32_t cellid, periphid;

  amba_read_id(amba_pl110_lcd_control_virt_base + 0xfe0, &periphid, &cellid);

  if ((periphid & 0xff0fffff) == 0x00041111 && cellid == 0xb105f00d)
    type = PL111;
  else if (periphid == 0x00041110 && cellid == 0xb105f00d)
    type = PL110;
  else
    {
      printf("Unknown LCD: periphid = %08x, cellid = %08x\n", periphid, cellid);
      type = PL_UNKNOWN;
    }
}

static const char *arm_lcd_get_info(void)
{
  return "ARM AMBA PrimeCell 11x";
}

static
l4_uint32_t read_sys_reg(unsigned reg)
{
  return *((volatile l4_uint32_t *)(amba_pl110_sys_base_virt + reg));
}

static
void write_sys_reg(unsigned reg, l4_uint32_t val)
{
  *((volatile l4_uint32_t *)(amba_pl110_sys_base_virt + reg)) = val;
}

static
void set_clcd_clock(l4_umword_t val)
{
  write_sys_reg(Reg_sys_lock, Sys_lock_unlock);
  write_sys_reg(Reg_sys_osc4, val);
  write_sys_reg(Reg_sys_lock, Sys_lock_lock);
}

static
l4_uint32_t read_clcd_reg(unsigned reg)
{
  return *((volatile l4_uint32_t *)(amba_pl110_lcd_control_virt_base + reg));
}

static
void write_clcd_reg(unsigned reg, l4_umword_t val)
{
  *((volatile l4_uint32_t *)(amba_pl110_lcd_control_virt_base + reg)) = val;
}

static
unsigned is_vexpress(void)
{
  return (read_sys_reg(Reg_sys_id) & 0xfff0000) == 0x1900000;
}

static
int init(unsigned long fb_phys_addr)
{
  l4_umword_t id;

  id = read_sys_reg(Reg_sys_clcd) & Sys_clcd_idmask;
  if (is_vexpress())
    id = Sys_clcd_id84;
  switch (id)
    {
    case Sys_clcd_id84:
    case Sys_clcd_idmask:
      printf("Configure 8.4 CLCD\n");
      if (use_xga)
        {
          set_clcd_clock(Sys_osc4_xga);
          write_clcd_reg(Reg_clcd_tim0, Clcd_tim0_84_xga);
          write_clcd_reg(Reg_clcd_tim1, Clcd_tim1_84_xga);
          write_clcd_reg(Reg_clcd_tim2, Clcd_tim2_84_xga);
          write_clcd_reg(Reg_clcd_tim3, Clcd_tim3_84_xga);
        }
      else
        {
          set_clcd_clock(Sys_osc4_25mhz);
          write_clcd_reg(Reg_clcd_tim0, Clcd_tim0_84_vga);
          write_clcd_reg(Reg_clcd_tim1, Clcd_tim1_84_vga);
          write_clcd_reg(Reg_clcd_tim2, Clcd_tim2_84_vga);
          write_clcd_reg(Reg_clcd_tim3, Clcd_tim3_84_vga);
        }
      break;

    case Sys_clcd_id38:
      printf("Configure 3.8 CLCD\n");
      set_clcd_clock(Sys_osc4_10mhz);
      write_clcd_reg(Reg_clcd_tim0, Clcd_tim0_38);
      write_clcd_reg(Reg_clcd_tim1, Clcd_tim1_38);
      write_clcd_reg(Reg_clcd_tim2, Clcd_tim2_38);
      write_clcd_reg(Reg_clcd_tim3, Clcd_tim3_38);
      break;

    case Sys_clcd_id25:
      printf("Configure 2.5 CLCD\n");
      set_clcd_clock(Sys_osc4_5p4mhz);
      write_clcd_reg(Reg_clcd_tim0, Clcd_tim0_25);
      write_clcd_reg(Reg_clcd_tim1, Clcd_tim1_25);
      write_clcd_reg(Reg_clcd_tim2, Clcd_tim2_25);
      write_clcd_reg(Reg_clcd_tim3, Clcd_tim3_25);

      // Turn the backlight on
      //write_ib2_reg(Reg_ib2_ctrl, read_ib2_reg(Reg_ib2_ctrl) | 0x01);
      break;

    default:
      printf("Error: Unknown display type (ID: %lx)\n", id >> 8);
      return 0;
  }

  // Set physical framebuffer address
  write_clcd_reg(Reg_clcd_ubas, fb_phys_addr);
  write_clcd_reg(Reg_clcd_lbas, 0);
  write_clcd_reg(Reg_clcd_ienb, 0);

  // Switch  power off and configure
  write_clcd_reg(Reg_clcd_cntl,
                 ((type == PL111 && config_use_565 && !is_qemu)
                  ? Clcd_cntl_lcdbpp16_pl111_565
                  : Clcd_cntl_lcdbpp16)
                 | Clcd_cntl_lcden | Clcd_cntl_lcdbw
                 | Clcd_cntl_lcdtft | Clcd_cntl_lcdvcomp
                 | (config_do_bgr ? Clcd_cntl_lcdbgr : 0));

  // Switch power on
  write_clcd_reg(Reg_clcd_cntl, read_clcd_reg(Reg_clcd_cntl) | Clcd_cntl_lcdpwr);

  return 1;
}

static void setup_memory(void)
{
  l4_size_t phys_size;
  l4io_device_handle_t dh;
  l4io_resource_handle_t hdl;

  if (fb_vaddr)
    return;

  if (l4io_lookup_device("System Control", &dh, 0, &hdl))
    {
      printf("Could not get system controller space\n");
      return;
    }

  /* System controller -- XXX Wrong Place XXX */
  amba_pl110_sys_base_virt
    = l4io_request_resource_iomem(dh, &hdl);
  if (amba_pl110_sys_base_virt == 0)
    {
      printf("Could not map system controller space\n");
      return;
    }

  if (l4io_lookup_device("AMBA PL110", &dh, 0, &hdl))
    {
      printf("Could not get PL110 LCD device\n");
      return;
    }

  amba_pl110_lcd_control_virt_base
    = l4io_request_resource_iomem(dh, &hdl);
  if (amba_pl110_lcd_control_virt_base == 0)
    {
      printf("Could not map controller space for '%s'\n", arm_lcd_get_info());
      return;
    }

  setup_type();

  if ((read_sys_reg(Reg_sys_clcd) & Sys_clcd_idmask) == 0x1000)
    {
      is_qemu = 1; // remember if we run on qemu because of the different
                   // handling of the bpp16 mode with PL110: my hardware has
                   // 5551 mode, qemu does 565
      type = PL111; // also set the type to PL111 because qemu only
                    // announces a PL110 but can do the 1024 resolution too
      printf("Running on QEmu (assuming PL111).\n");
    }

  if (config_request_xga && type == PL111)
    use_xga = 1;

  // get some frame buffer
  l4re_ds_t mem = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(mem))
    return;

  if (l4re_ma_alloc(fbmem_size(), mem, L4RE_MA_CONTINUOUS | L4RE_MA_PINNED))
    {
      printf("Error allocating memory\n");
      return;
    }

  fb_vaddr = 0;
  if (l4re_rm_attach(&fb_vaddr, fbmem_size(),
                     L4RE_RM_SEARCH_ADDR | L4RE_RM_EAGER_MAP,
                     mem, 0, L4_PAGESHIFT))
    {
      printf("Error getting memory\n");
      return;
    }

  printf("Video memory is at virtual %p (size: 0x%x Bytes)\n",
         fb_vaddr, fbmem_size());

  // get physical address
  if (l4re_ds_phys(mem, 0, &fb_paddr, &phys_size)
      || phys_size != fbmem_size())
    {
      printf("Getting the physical address failed or not contiguous\n");
      return;
    }
  printf("Physical video memory is at %p\n", (void *)fb_paddr);
}

static void *fb(void)
{
  if (!fb_vaddr)
    setup_memory();

  return fb_vaddr;
}

static void pl110_enable(void)
{
  const char *s;

  setup_memory();

  switch (type)
    {
      case PL110: s = "ARM AMBA PrimeCell PL110"; break;
      case PL111: s = "ARM AMBA PrimeCell PL111"; break;
      default: s = "Unknown"; break;
    }

  printf("Detected a '%s' device.\n", s);

  if (!fb_vaddr || !init(fb_paddr))
    {
      printf("CLCD init failed!\n");
      return;
    }
}

static void pl110_disable(void)
{
}
static struct arm_lcd_ops arm_lcd_ops_pl11x = {
  .probe              = probe,
  .get_fbinfo         = get_fbinfo,
  .get_fb             = fb,
  .get_video_mem_size = fbmem_size,
  .get_info           = arm_lcd_get_info,
  .enable             = pl110_enable,
  .disable            = pl110_disable,
};

arm_lcd_register(&arm_lcd_ops_pl11x);
