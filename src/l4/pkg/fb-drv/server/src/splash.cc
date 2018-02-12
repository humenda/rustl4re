/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include <l4/re/video/colors>

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "splash.h"

void splash_display(L4Re::Video::View::Info *fb_info, l4_addr_t fbaddr)
{
  if (fb_info->width < SPLASHNAME.width)
    return;
  if (fb_info->height < SPLASHNAME.height)
    return;

  int size = SPLASHNAME.width * SPLASHNAME.height;
  unsigned char *buf = (unsigned char *)malloc(size * SPLASHNAME.bytes_per_pixel);
  if (!buf)
    return;

  SPLASHNAME_RUN_LENGTH_DECODE(buf, SPLASHNAME.rle_pixel_data, size,
                               SPLASHNAME.bytes_per_pixel);

  int buf_idx = 0;
  int off_x = (fb_info->width / 2) - (SPLASHNAME.width / 2);
  int off_y = (fb_info->height / 2) - (SPLASHNAME.height / 2);
  char *_fb = (char *)fbaddr + off_y * fb_info->bytes_per_line
              + off_x * fb_info->pixel_info.bytes_per_pixel();
  char *a;
  unsigned x;

  for (unsigned y = 0; y < SPLASHNAME.height;
       ++y, _fb += fb_info->bytes_per_line)
    for (x = 0, a = _fb; x < SPLASHNAME.width;
         ++x, a += fb_info->pixel_info.bytes_per_pixel())
      {
        unsigned valr = buf[buf_idx];
        unsigned valg = buf[buf_idx + 1];
        unsigned valb = buf[buf_idx + 2];
        unsigned v;

        v  = (((valr <<  0) >> (8  - fb_info->pixel_info.r().size())) & ((1 << fb_info->pixel_info.r().size()) - 1)) << fb_info->pixel_info.r().shift();
        v |= (((valg <<  8) >> (16 - fb_info->pixel_info.g().size())) & ((1 << fb_info->pixel_info.g().size()) - 1)) << fb_info->pixel_info.g().shift();
        v |= (((valb << 16) >> (24 - fb_info->pixel_info.b().size())) & ((1 << fb_info->pixel_info.b().size()) - 1)) << fb_info->pixel_info.b().shift();

        switch (fb_info->pixel_info.bytes_per_pixel())
          {
          case 1: *(unsigned char  *)a = v; break;
          case 2: *(unsigned short *)a = v; break;
          case 3: memcpy(a, &v, 3); break;
          case 4: *(unsigned int   *)a = v; break;
          };

        buf_idx += SPLASHNAME.bytes_per_pixel;
      }

  free(buf);
}
