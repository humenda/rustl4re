/**
 * \file
 * \brief     Font functions
 *
 * \author    Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *            Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *            Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2001-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/libgfxbitmap/font.h>
#include <l4/libgfxbitmap/bitmap.h>
#include <l4/sys/l4int.h>
#include <string.h>
#include <stdio.h>

extern const char _binary_vgafont_psf_start[];
extern const char _binary_lat0_12_psf_start[];
extern const char _binary_lat0_14_psf_start[];
extern const char _binary_lat0_16_psf_start[];

static struct {
  const char *fontdata;
  const char *name;
} font_list[] = {
  { _binary_lat0_14_psf_start, "lat0-14" }, // first one is the default one
  { _binary_lat0_12_psf_start, "lat0-12" },
  { _binary_lat0_16_psf_start, "lat0-16" },
  { _binary_vgafont_psf_start, "vgafont" },
};

enum {
  FONT_XRES = 8,
};

struct psf_header
{
  unsigned char magic[2];
  unsigned char mode;
  unsigned char height;
} __attribute__((packed));

struct psf_font
{
  struct psf_header header;
  char data[];
};

static struct psf_font *std_font;


static int check_magic(struct psf_font *f)
{ return f->header.magic[0] == 0x36 && f->header.magic[1] == 0x4; }

static inline struct psf_font *font_cast(gfxbitmap_font_t font)
{
  struct psf_font *f = (struct psf_font *)font;
  if (!f || !check_magic(f))
    return std_font;
  return f;
}

static unsigned char font_yres(struct psf_font *f)
{ return f->header.height; }

L4_CV unsigned
gfxbitmap_font_width(gfxbitmap_font_t font)
{
  (void)font;
  return FONT_XRES;
}

L4_CV unsigned
gfxbitmap_font_height(gfxbitmap_font_t font)
{
  struct psf_font *f = font_cast(font);
  return f ? f->header.height : 0;
}

static inline
void *
get_font_char(struct psf_font *f, unsigned c)
{
  return &f->data[(FONT_XRES / 8) * font_yres(f) * c];
}

L4_CV void *
gfxbitmap_font_data(gfxbitmap_font_t font, unsigned c)
{
  struct psf_font *f = font_cast(font);
  if (!f)
    return NULL;

  return get_font_char(f, c);
}

L4_CV void
gfxbitmap_font_text(void *fb, l4re_video_view_info_t *vi,
                    gfxbitmap_font_t font, const char *text, unsigned len,
                    unsigned x, unsigned y,
                    gfxbitmap_color_pix_t fg, gfxbitmap_color_pix_t bg)
{
  unsigned i, j;
  struct gfxbitmap_offset offset = {0,0,0};
  struct psf_font *f = font_cast(font);

  if (!f)
    return;

  if (len == GFXBITMAP_USE_STRLEN)
    len = strlen(text);

  for (i = 0; i < len; i++, text++)
    {
      /* optimization: collect spaces */
      for (j = 0; (i < len) && (*text == ' '); i++, j++, text++)
        ;

      if (j > 0)
        {
          gfxbitmap_fill(fb, vi, x, y, j * FONT_XRES, font_yres(f), bg);
          x += j * FONT_XRES;
          i--;
          text--;
          continue;
        }

      gfxbitmap_bmap(fb, vi, x, y, FONT_XRES, font_yres(f),
                     get_font_char(f, *(unsigned char *)text), fg, bg,
                     &offset, pSLIM_BMAP_START_MSB);
      x += FONT_XRES;
    }
}


L4_CV void
gfxbitmap_font_text_scale(void *fb, l4re_video_view_info_t *vi,
                          gfxbitmap_font_t font, const char *text, unsigned len,
                          unsigned x, unsigned y,
                          gfxbitmap_color_pix_t fg, gfxbitmap_color_pix_t bg,
                          int scale_x, int scale_y)
{
  int pix_x, pix_y;
  unsigned rect_x = x;
  unsigned rect_y = y;
  unsigned rect_w = gfxbitmap_font_width(font) * scale_x;
  unsigned i;
  struct psf_font *f = font_cast(font);

  if (!f)
    return;

  pix_x = scale_x;
  if (scale_x >= 5)
    pix_x = scale_x * 14/15;
  pix_y = scale_y;
  if (scale_y >= 5)
    pix_y = scale_y * 14/15;

  if (len == GFXBITMAP_USE_STRLEN)
    len = strlen(text);

  for (i=0; i < len; i++, text++)
    {
      unsigned lrect_x = rect_x;
      unsigned lrect_y = rect_y;
      unsigned lrect_w = pix_x;
      unsigned lrect_h = pix_y;
      const char *bmap = get_font_char(f, *text);
      int j;

      for (j=0; j<font_yres(f); j++)
        {
          unsigned char mask = 0x80;
          int k;

          for (k=0; k<FONT_XRES; k++)
            {
              unsigned color = (*bmap & mask) ? fg : bg;
              gfxbitmap_fill(fb, vi, lrect_x, lrect_y, lrect_w, lrect_h, color);
              lrect_x += scale_x;
              bmap += (mask &  1);
              mask  = (mask >> 1) | (mask << 7);
            }
          lrect_x -= rect_w;
          lrect_y += scale_y;
        }
      rect_x += rect_w;
    }
}


L4_CV gfxbitmap_font_t
gfxbitmap_font_get(const char *name)
{
  unsigned i = 0;
  for (; i < sizeof(font_list) / sizeof(font_list[0]); ++i)
    if (!strcmp(font_list[i].name, name))
      return (gfxbitmap_font_t)font_list[i].fontdata;
  return NULL;
}

/** Init lib */
L4_CV int
gfxbitmap_font_init(void)
{
  unsigned chars;

  struct psf_font *f;
  f = font_cast((gfxbitmap_font_t)font_list[0].fontdata);

  /* check magic number of .psf */
  if (!check_magic(f))
    return 1; // psf magic number failed

  std_font = f;

  /* check file mode */
  switch (f->header.mode)
    {
    case 0:
    case 2:
      chars = 256;
      break;
    case 1:
    case 3:
      chars = 512;
      break;
    default:
      return 2; // "bad psf font file magic %02x!", f->header.mode
    }

  if (0)
    printf("Font: Character size is %dx%d, font has %d characters\n",
           FONT_XRES, font_yres(f), chars);

  return 0;
}

