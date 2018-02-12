/*
 * Tegra2 FB driver (just pass-through, must be init'ed by boot-loader)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <l4/drivers/lcd.h>
#include <l4/io/io.h>
#include <l4/util/util.h>

static inline int width(void) { return 1280; }
static inline int height(void) { return 1024; }
static inline int bytes_per_pixel(void) { return 2; }

static unsigned int fbmem_size(void)
{ return height() * width() * bytes_per_pixel(); }

static void *fb_vaddr;

static void setup_memory(void)
{
  l4_addr_t fb_paddr;
  l4_addr_t a = 0;
  int ret;

  if (fb_vaddr)
    return;

  fb_paddr = 0x1c012000;
  ret = l4io_request_iomem(fb_paddr, 0x500000, 0, &a);
  if (ret)
    {
      printf("[LCD] Error: Could not map device memory\n");
      return;
    }

  fb_vaddr = (void *)a;

  printf("[LCD] Info: Video memory is at virtual %p (size: 0x%x Bytes)\n",
         fb_vaddr, fbmem_size());
  printf("[LCD] Info: Physical video memory is at %p\n", (void *)fb_paddr);
}


static int lcd_probe(const char *configstr)
{
  (void)configstr;
  return !l4io_lookup_device("FBMEM", NULL, 0, 0);
}

static void *lcd_get_fb(void)
{
  if (!fb_vaddr)
    setup_memory();

  return fb_vaddr;
}

static unsigned int lcd_fbmem_size(void) { return fbmem_size(); }

static const char *lcd_get_info(void)
{
  return "TEGRA2 FASTBOOT init'ed FB";
}

static int get_fbinfo(l4re_video_view_info_t *vinfo)
{
  vinfo->width               = width();
  vinfo->height              = height();
  vinfo->bytes_per_line      = bytes_per_pixel() * vinfo->width;

  vinfo->pixel_info.bytes_per_pixel = bytes_per_pixel();
  vinfo->pixel_info.r.shift         = 11;
  vinfo->pixel_info.r.size          = 5;
  vinfo->pixel_info.g.shift         = 5;
  vinfo->pixel_info.g.size          = 6;
  vinfo->pixel_info.b.shift         = 0;
  vinfo->pixel_info.b.size          = 5;
  vinfo->pixel_info.a.shift         = 0;
  vinfo->pixel_info.a.size          = 0;
  return 0;
}

static void lcd_enable(void)
{
  setup_memory();
}

static void lcd_disable(void)
{
  printf("%s unimplemented.\n", __func__);
}

static struct arm_lcd_ops arm_lcd_ops_omap3 = {
  .probe              = lcd_probe,
  .get_fb             = lcd_get_fb,
  .get_fbinfo         = get_fbinfo,
  .get_video_mem_size = lcd_fbmem_size,
  .get_info           = lcd_get_info,
  .enable             = lcd_enable,
  .disable            = lcd_disable,
};

arm_lcd_register(&arm_lcd_ops_omap3);
