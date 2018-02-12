/*
 * LCD driver for s3c2410
 */

#include <l4/drivers/lcd.h>
#include <stdlib.h>
#include <stdio.h>

#include <l4/re/c/dataspace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/io/io.h>

#define LCD_NAME "LCD s3c2410"

static void *fb_vaddr;
static l4_addr_t fb_paddr;

enum {
  LCDCON1   = 0x00,
  LCDCON2   = 0x04,
  LCDCON3   = 0x08,
  LCDCON4   = 0x0c,
  LCDCON5   = 0x10,
  LCDSADDR1 = 0x14,
  LCDSADDR2 = 0x18,
  LCDSADDR3 = 0x1c,

  WIDTH = 480,
  HEIGHT = 640,

  LCDCON1_ENABLE_BIT    = 1,
  LCDCON1_BPPMODE_16BPP = 0xc << 1,
  LCDCON1_PNRMODE_LCD   = 3 << 5,
  LCDCON1_CLKVAL        = 0,
  LCDCON1_ENABLE_VALUE  = LCDCON1_ENABLE_BIT | LCDCON1_BPPMODE_16BPP
                             | LCDCON1_PNRMODE_LCD | LCDCON1_CLKVAL,

  LCDCON2_LINEVAL = (HEIGHT-1) << 14,
  LCDCON2_ENABLE_VALUE = LCDCON2_LINEVAL,

  LCDCON3_HOZVAL = (WIDTH-1) << 8,
  LCDCON3_ENABLE_VALUE = LCDCON3_HOZVAL,

  LCDCON4_ENABLE_VALUE = 7,

  LCDCON5_PWREN  = 1 << 3,
  LCDCON5_FRM565 = 1 << 11,
  LCDCON5_ENABLE_VALUE = LCDCON5_PWREN | LCDCON5_FRM565 | 1,


//  wr(0x00000179 & ~LCDCON1_ENABLE_BIT, LCDCON1);
//  wr(0x019fc3c1, LCDCON2);
//  wr(0x0039df67, LCDCON3);
//  wr(0x00000007, LCDCON4);
//  wr(0x00000f09, LCDCON5);
};

static const char *arm_lcd_get_info(void)
{ return "S3C2410"; }

static int probe(const char *configstr)
{
  (void)configstr;
  return !l4io_lookup_device(LCD_NAME, NULL, 0, 0);
}

static unsigned int fbmem_size(void) { return (1 << 22); } //height() * bpl(); }

static unsigned long lcd_control_virt_base;

static int get_fbinfo(l4re_video_view_info_t *vinfo)
{
  vinfo->width          = 480;
  vinfo->width          = 640;
  vinfo->bytes_per_line = 2 * vinfo->width;

  vinfo->pixel_info.bytes_per_pixel = 2;
  vinfo->pixel_info.r.shift         = 0;
  vinfo->pixel_info.r.size          = 5;
  vinfo->pixel_info.g.shift         = 5;
  vinfo->pixel_info.g.size          = 6;
  vinfo->pixel_info.b.shift         = 11;
  vinfo->pixel_info.b.size          = 5;
  vinfo->pixel_info.a.shift         = 0;
  vinfo->pixel_info.a.size          = 0;

  return 0;
}


static void setup_memory(void)
{
  l4_size_t phys_size;
  l4io_device_handle_t dh;
  l4io_resource_handle_t hdl;


  if (fb_vaddr)
    return;

  if (l4io_lookup_device(LCD_NAME, &dh, 0, &hdl))
    {
      printf("Could not get s3c2410fb\n");
      return;
    }
  lcd_control_virt_base = l4io_request_resource_iomem(dh, &hdl);

  if (lcd_control_virt_base == 0)
    {
      printf("Could not map controller space for '%s'\n", arm_lcd_get_info());
      return;
    }

  // get some frame buffer
  l4re_ds_t mem = l4re_util_cap_alloc();
  if (l4_is_invalid_cap(mem))
    return;

  if (l4re_ma_alloc(fbmem_size(), mem, L4RE_MA_CONTINUOUS | L4RE_MA_PINNED))
    {
      printf("Could not get video memory.\n");
      return;
    }

  fb_vaddr = 0;
  if (l4re_rm_attach(&fb_vaddr, fbmem_size(),
                     L4RE_RM_SEARCH_ADDR | L4RE_RM_EAGER_MAP,
                     mem, 0, L4_PAGESHIFT))

    {
      printf("Could not map fb\n");
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

static inline void wr(unsigned long val, unsigned long regoff)
{ *(volatile unsigned long *)(lcd_control_virt_base + regoff) = val; }
static inline unsigned long rd(unsigned long regoff)
{ return *(volatile unsigned long *)(lcd_control_virt_base + regoff); }

static void *fb(void)
{
  if (!fb_vaddr)
    setup_memory();

  return fb_vaddr;
}

#if 0
s3c2410fb: devinit
s3c2410fb: got LCD region
s3c2410fb: got and enabled clock
s3c2410fb: map_video_memory(fbi=c04ce274)
  s3c2410fb: map_video_memory: clear ffc00000:0012c000
  s3c2410fb: map_video_memory: dma=30600000 cpu=ffc00000 size=0012c000
  s3c2410fb: got video memory
  s3c2410fb: LCDSADDR1 = 0x18300000
  s3c2410fb: LCDSADDR2 = 0x1834b000
  s3c2410fb: LCDSADDR3 = 0x000001e0
  s3c2410fb: LPCSEL    = 0x00000cf0
  s3c2410fb: replacing TPAL 00000000
  s3c2410fb: check_var(var=c04ce008, info=c04ce000)
  s3c2410fb: s3c2410fb_activate_var: var->xres  = 480
  s3c2410fb: s3c2410fb_activate_var: var->yres  = 640
  s3c2410fb: s3c2410fb_activate_var: var->bpp   = 16
  s3c2410fb: setting vert: up=2, low=16, sync=2
  s3c2410fb: setting horz: lft=104, rt=8, sync=8
  s3c2410fb: new register set:
  s3c2410fb: lcdcon[1] = 0x00000179
  s3c2410fb: lcdcon[2] = 0x019fc3c1
  s3c2410fb: lcdcon[3] = 0x0039df67
  s3c2410fb: lcdcon[4] = 0x00000007
  s3c2410fb: lcdcon[5] = 0x00000f09
  s3c2410fb: LCDSADDR1 = 0x18300000
  s3c2410fb: LCDSADDR2 = 0x1834b000
  s3c2410fb: LCDSADDR3 = 0x000001e0
  Console: switching to colour frame buffer device 80x58
  fb0: s3c2410fb frame buffer device
#endif

static void pl110_enable(void)
{
  setup_memory();

  wr(0x00000179 & ~LCDCON1_ENABLE_BIT, LCDCON1);
  wr(0x019fc3c1, LCDCON2);
  wr(0x0039df67, LCDCON3);
  wr(0x00000007, LCDCON4);
  wr(0x00000f09, LCDCON5);
#if 0
  wr(LCDCON1_ENABLE_VALUE & ~LCDCON1_ENABLE_BIT, LCDCON1);
  wr(LCDCON2_ENABLE_VALUE, LCDCON2);
  wr(LCDCON3_ENABLE_VALUE, LCDCON3);
  wr(LCDCON4_ENABLE_VALUE, LCDCON4);
  wr(LCDCON5_ENABLE_VALUE, LCDCON5);
#endif


  wr(fb_paddr >> 1, LCDSADDR1);
  wr((fb_paddr + 640*480*2) >> 1, LCDSADDR2);
  wr(480, LCDSADDR3);

  wr(0x00000179, LCDCON1);
  {
//    int i = 0;
 //   for (; i < 0x20; i+=4)
  //    printf("%02x: %08lx\n", i, rd(i));
  }
}

static void pl110_disable(void)
{
}
static struct arm_lcd_ops arm_lcd_ops_pl110 = {
  .probe              = probe,
  .get_fb             = fb,
  .get_fbinfo         = get_fbinfo,
  .get_video_mem_size = fbmem_size,
  .get_info           = arm_lcd_get_info,
  .enable             = pl110_enable,
  .disable            = pl110_disable,
};

arm_lcd_register(&arm_lcd_ops_pl110);
