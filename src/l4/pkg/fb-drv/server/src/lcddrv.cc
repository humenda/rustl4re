/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/lcd.h>
#include <l4/sys/cache.h>
#include <cstdio>

#include "fb.h"
#include "splash.h"

class Lcd_drv_fb : public Phys_fb
{
public:
  bool setup_drv(Prog_args *pa, L4Re::Util::Object_registry *);
};


Phys_fb *Phys_fb::probe()
{
  return new Lcd_drv_fb();
}


bool
Lcd_drv_fb::setup_drv(Prog_args *pa, L4Re::Util::Object_registry *)
{
  struct arm_lcd_ops *lcd;

  if (!(lcd = arm_lcd_probe(pa->config_str)))
    {
      printf("Could not find LCD.\n");
      return false;
    }

  printf("Using LCD driver: %s\n", lcd->get_info());

  _vidmem_start = (l4_addr_t)lcd->get_fb();

  if (!_vidmem_start)
    {
      printf("Driver init failed\n");
      return false;
    }

  _vidmem_size  = lcd->get_video_mem_size();
  _vidmem_end   = _vidmem_start + _vidmem_size;

  if (lcd->get_fbinfo((l4re_video_view_info_t *)&_view_info))
    {
      printf("Failed to get driver framebuffer info\n");
      return false;
    }

  _screen_info.width = _view_info.width;
  _screen_info.height = _view_info.height;
  _screen_info.flags = L4Re::Video::Goos::F_auto_refresh;
  _screen_info.pixel_info = _view_info.pixel_info;
  _view_info.buffer_offset = 0;

  init_infos();

  lcd->enable();

  splash_display(&_view_info, _vidmem_start);

  // slow
  //l4_cache_dma_coherent(_vidmem_start, _vidmem_start + _vidmem_size);
  l4_cache_dma_coherent_full();
  return true;
}
