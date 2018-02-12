/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */


#include <l4/libgfxbitmap/bitmap.h>
#include <l4/re/video/view>
#include <l4/libgfxbitmap/support>


// array will be converted to gfxbitmap_color_pix_t in init
gfxbitmap_color_t libterm_std_colors[24] =
{
  // dark
  0x00000000, 0x007f0000, 0x00007f00, 0x007f7f00,
  0x0000007f, 0x007f007f, 0x00007f7f, 0x007f7f7f,
  // normal
  0x00000000, 0x00cf0000, 0x0000cf00, 0x00cfcf00,
  0x000000cf, 0x00cf00cf, 0x0000cfcf, 0xcfcfcfcf,
  // bright
  0x00000000, 0x00ff0000, 0x0000ff00, 0x00ffff00,
  0x000000ff, 0x00ff00ff, 0x0000ffff, 0x00ffffff
};


void
libterm_init_colors(L4Re::Video::View::Info *vi)
{
  unsigned i = 0;
  for (; i < (sizeof(libterm_std_colors) / sizeof(libterm_std_colors[0])); ++i)
    libterm_std_colors[i] = gfxbitmap_convert_color((l4re_video_view_info_t *)vi, libterm_std_colors[i]);
}

int
libterm_get_color(int mode, int color)
{
  if (mode >= 3 || color >= 8)
    return 0; // black

  return libterm_std_colors[color + 8 * mode];
}
