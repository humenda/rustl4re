/*
 * Dummy LCD driver
 */

#include <l4/drivers/lcd.h>
#include <stdlib.h>

static const char *arm_lcd_none_get_info(void)
{ return "ARM LCD virtual driver"; }

static void void_dummy(void) {}

static int probe(const char *c)             { (void)c; return 0; }
static unsigned int mem_size(void) { return 200 * 320 * 4; }
static void *fb(void)              { return malloc(mem_size()); }

static int get_fbinfo(l4re_video_view_info_t *vinfo)
{
  vinfo->width          = 320;
  vinfo->width          = 200;
  vinfo->bytes_per_line = 4 * vinfo->width;

  vinfo->pixel_info.bytes_per_pixel = 4;
  vinfo->pixel_info.r.shift         = 0;
  vinfo->pixel_info.r.size          = 8;
  vinfo->pixel_info.g.shift         = 8;
  vinfo->pixel_info.g.size          = 8;
  vinfo->pixel_info.b.shift         = 16;
  vinfo->pixel_info.b.size          = 8;
  vinfo->pixel_info.a.shift         = 0;
  vinfo->pixel_info.a.size          = 0;

  return 0;
}

static struct arm_lcd_ops arm_lcd_ops_virtual = {
  .probe              = probe,
  .get_fbinfo         = get_fbinfo,
  .get_fb             = fb,
  .get_video_mem_size = mem_size,
  .get_info           = arm_lcd_none_get_info,
  .enable             = void_dummy,
  .disable            = void_dummy,
};

arm_lcd_register(&arm_lcd_ops_virtual);
