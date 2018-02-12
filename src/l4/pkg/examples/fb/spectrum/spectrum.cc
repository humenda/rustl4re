/**
 * \file
 * \brief	fb usage demo, with C and C++
 * \author      Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 **/

/* (c) 2010, Adam Lackorzynski
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

/*
 * Event handling in C mode has not been done.
 */

#ifdef DO_C
#include <l4/re/c/util/video/goos_fb.h>
#else
#include <l4/re/util/video/goos_fb>
#include <l4/sys/semaphore>
#include <l4/re/util/event>
#include <l4/event/event>
#endif

#include <l4/re/event_enums.h>
#include <l4/util/keymap.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef DO_C
static l4re_util_video_goos_fb_t gfb;
static l4re_video_view_info_t fbi;
#else
static L4Re::Util::Video::Goos_fb gfb;
static L4Re::Video::View::Info fbi;
static L4Re::Util::Event event;
#endif

static void *fbmem_vaddr;
static unsigned bpp;

static void put_pixel(int x, int y, int fullval)
{
  unsigned v = 0;
#ifdef DO_C
  unsigned bpp = l4re_video_bits_per_pixel(&fbi.pixel_info);
  unsigned long offset = (unsigned long)fbmem_vaddr + y * fbi.bytes_per_line + x * fbi.pixel_info.bytes_per_pixel;
  v  = ((fullval >> (8  - fbi.pixel_info.r.size)) & ((1 << fbi.pixel_info.r.size) - 1)) << fbi.pixel_info.r.shift;
  v |= ((fullval >> (16 - fbi.pixel_info.g.size)) & ((1 << fbi.pixel_info.g.size) - 1)) << fbi.pixel_info.g.shift;
  v |= ((fullval >> (24 - fbi.pixel_info.b.size)) & ((1 << fbi.pixel_info.b.size) - 1)) << fbi.pixel_info.b.shift;
#else
  unsigned long offset = (unsigned long)fbmem_vaddr + y * fbi.bytes_per_line + x * fbi.pixel_info.bytes_per_pixel();
  v  = ((fullval >> (8  - fbi.pixel_info.r().size())) & ((1 << fbi.pixel_info.r().size()) - 1)) << fbi.pixel_info.r().shift();
  v |= ((fullval >> (16 - fbi.pixel_info.g().size())) & ((1 << fbi.pixel_info.g().size()) - 1)) << fbi.pixel_info.g().shift();
  v |= ((fullval >> (24 - fbi.pixel_info.b().size())) & ((1 << fbi.pixel_info.b().size()) - 1)) << fbi.pixel_info.b().shift();
#endif

  switch (bpp)
    {
    case 8: *(unsigned char  *)offset = v; break;
    case 14: case 15: case 16: *(unsigned short *)offset = v; break;
    case 24: case 32: *(unsigned int   *)offset = v; break;
    default:
      printf("unhandled bitsperpixel %d\n", bpp);
    };
}

static void update_rect(int x, int y, int w, int h)
{
#ifdef DO_C
  l4re_util_video_goos_fb_refresh(&gfb, x, y, w, h);
#else
  gfb.refresh(x, y, w, h);
#endif
}

static inline unsigned color_val(unsigned w, unsigned peak_point, unsigned val)
{
  unsigned third = w / 3;

  if (third == 0)
    return 0;

  unsigned a = abs(val - peak_point);
  if (a > third * 2)
    a = peak_point + w - val;
  if (a > third)
    return 0;

  return ((third - a) * 255) / third;
}

#ifndef DO_C
namespace {
struct Ev_loop : public Event::Event_loop
{
  Ev_loop(L4::Cap<L4::Semaphore> irq, int prio) : Event::Event_loop(irq, prio) {}
  void handle();
};

void Ev_loop::handle()
{
  while (L4Re::Event_buffer::Event *e = event.buffer().next())
    {
      int k;
      printf("Event: %16lld: %d %d %d\n",
             e->time, e->payload.type, e->payload.code, e->payload.value);
      if (e->payload.type == L4RE_EV_KEY
          && ((k = l4util_map_event_to_keymap(e->payload.code, 0)) >= 32))
        printf("   key: %c\n", k);
      // proper mouse and keyboard handling code comes here
      e->free();
    }
}
}
#endif

int main(void)
{
#ifndef DO_C
  try { gfb.setup("fb"); } catch (...) { return 1; }
  if (gfb.view_info(&fbi))
    return 2;

  if (!(fbmem_vaddr = gfb.attach_buffer()))
    return 3;

  bpp = fbi.pixel_info.bits_per_pixel();

  if (auto ev = L4::cap_dynamic_cast<L4Re::Event>(gfb.goos()))
    {
      if (event.init<L4::Semaphore>(ev))
        return 4;

      // use new so that Ev_loop survives this block
      Ev_loop *event_hdl;
      event_hdl = new Ev_loop(L4::cap_cast<L4::Semaphore>(event.irq()), 4);
      if (!event_hdl->attached())
        return 5;
      event_hdl->start();
    }
  else
    printf("Goos cap does not support event protocol, running without.\n");

#else
  if (l4re_util_video_goos_fb_setup_name(&gfb, "fb"))
    return 1;

  if (l4re_util_video_goos_fb_view_info(&gfb, &fbi))
    return 2;

  if (!(fbmem_vaddr = l4re_util_video_goos_fb_attach_buffer(&gfb)))
    return 3;

   bpp = l4re_video_bits_per_pixel(&fbi.pixel_info);
#endif

  printf("x:%ld y:%ld bit/pixel:%d bytes/line:%ld\n",
         fbi.width, fbi.height, bpp, fbi.bytes_per_line);

  // now some fancy stuff
  unsigned w = fbi.width;
  unsigned h = fbi.height;
  unsigned t = w / 3;

  for (unsigned cnt = 0; ; cnt += 2)
    {
      for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
          {
            unsigned r = color_val(w, 0 * t, (x + cnt) % w);
            unsigned g = color_val(h, 1 * t, (y + (cnt >> 1)) % h);
            unsigned b = color_val(w, 2 * t, (w - x + cnt) % w);

            if (0)
              printf("%3d: %3d:%3d:%3d\n", x, r, g, b);
            put_pixel(x, y, (r << 0) | (g << 8) | (b << 16));
          }

      update_rect(0, 0, fbi.width, fbi.height);
      usleep(100000);
    }

  return 0;
}
