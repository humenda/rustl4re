/**
 * \file
 * \brief  L4 Console
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/l4con/l4con.h>


/* C part */

L4_CV long
l4con_close(l4re_fb_t fb) L4_NOTHROW
{
  L4::Cap<L4con> f(fb);
  return f->close();
}

L4_CV long
l4con_pslim_fill(l4re_fb_t fb, int x, int y, int w, int h, unsigned int color) L4_NOTHROW
{
  L4::Cap<L4con> f(fb);
  return f->pslim_fill(x, y, w, h, color);
}

L4_CV long
l4con_puts(l4re_fb_t fb, const char *s, unsigned long len, short x, short y,
             unsigned int fg_color, unsigned int bg_color) L4_NOTHROW
{
  L4::Cap<L4con> f(fb);
  return f->puts(s, len, x, y, fg_color, bg_color);
}

L4_CV long
l4con_puts_scale(l4re_fb_t fb, const char *s, unsigned long len,
                 short x, short y,
                 unsigned int fg_color, unsigned int bg_color,
                 short scale_x, short scale_y) L4_NOTHROW
{
  L4::Cap<L4con> f(fb);
  return f->puts_scale(s, len, x, y, fg_color, bg_color, scale_x, scale_y);
}

L4_CV long
l4con_get_font_size(l4re_fb_t fb, unsigned int *fn_w, unsigned int *fn_h) L4_NOTHROW
{
  L4::Cap<L4con> f(fb);
  return f->get_font_size(fn_w, fn_h);
}

