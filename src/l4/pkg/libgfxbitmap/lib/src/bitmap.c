/**
 * \file
 * \brief	bitmap functions
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *		Adam Lackorzynski <adam@os.inf.tu-dresden.de> */
/*
 * (c) 2001-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <stdlib.h>
#include <string.h>		/* needed for memmove */

#include <l4/sys/cache.h>

#include <l4/libgfxbitmap/bitmap.h>

#ifdef ARCH_x86
static int use_fastmemcpy;
#endif

#define OFFSET(x, y, ptr, bytepp) ptr += (y) * bwidth + (x) * (bytepp);

static inline void
_bmap16lsb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
   l4_uint32_t nobits=0;
   l4_uint32_t i, j, k, kmod;

   nobits += offset->preskip_y
      * (w + offset->preskip_x + offset->endskip_x);
   /* length of one line in bmap (bits!) */

   for (i = 0; i < h; i++) {
      nobits += offset->preskip_x;
      for (j = 0; j < w; j++, nobits++) {
	 k = nobits>>3;
	 kmod = (nobits)%8;
	 if ( bmap[k] & (0x01 << kmod) )
	    *(l4_uint16_t*) (&vfb[2*j]) = (l4_uint16_t) (fgc & 0xffff);
	 else
	    *(l4_uint16_t*) (&vfb[2*j]) = (l4_uint16_t) (bgc & 0xffff);
      }
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*2);
#endif
      vfb += bwidth;
   }
}

static inline void
_bmap16msb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
  l4_uint32_t i, j;
  l4_uint32_t nobits = offset->preskip_y
		   * (w + offset->preskip_x + offset->endskip_x);

  for (i = 0; i < h; i++)
    {
      unsigned char mask, *b;
      nobits += offset->preskip_x;
      mask = 0x80 >> (nobits % 8);
      b = bmap + nobits / 8;
      for (j = 0; j < w; j++, nobits++)
	{
	  /* gcc is able to code the entire loop without using any jump
	   * if compiled with -march=i686 (uses cmov instructions then) */
	  *(l4_uint16_t*) (&vfb[2*j]) = (*b & mask)
					? (l4_uint16_t) (fgc & 0xffff)
					: (l4_uint16_t) (bgc & 0xffff);
	  b += mask & 1;
	  mask = (mask >> 1) | (mask << 7); /* gcc optimizes this into ROR */
	}
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*2);
#endif
      vfb += bwidth;
      nobits += offset->endskip_x;
   }
}

static inline void
_bmap24lsb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
   l4_uint32_t nobits=0;
   l4_uint32_t i,j, k,kmod;

   nobits += offset->preskip_y
      * (w + offset->preskip_x + offset->endskip_x);
   /* length of one line in bmap (bits!) */

   for (i = 0; i < h; i++) {
      nobits += offset->preskip_x;
      for (j = 0; j < w; j++, nobits++) {
	 k = nobits>>3;
	 kmod = (nobits)%8;
	 if ( bmap[k] & (0x01 << kmod) ) {
	    *(l4_uint16_t*) (&vfb[3*j]) = (l4_uint16_t) (fgc & 0xffff);
	    vfb[3*j+2] = (l4_uint8_t) (fgc >> 16);
	 }
	 else {
	    *(l4_uint16_t*) (&vfb[3*j]) = (l4_uint16_t) (bgc & 0xffff);
	    vfb[3*j+2] = (l4_uint8_t) (bgc >> 16);
	 }
      }
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*3);
#endif
      vfb += bwidth;
   }
}

static inline void
_bmap24msb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
   l4_uint32_t nobits=0;
   l4_uint32_t i,j, k,kmod;

   nobits += offset->preskip_y
      * (w + offset->preskip_x + offset->endskip_x);
   /* length of one line in bmap (bits!) */

   for (i = 0; i < h; i++) {
      nobits += offset->preskip_x;
      for (j = 0; j < w; j++, nobits++) {
	 k = nobits>>3;
	 kmod = (nobits)%8;
	 if ( bmap[k] & (0x80 >> kmod) ) {
	    *(l4_uint16_t*) (&vfb[3*j]) = (l4_uint16_t) (fgc & 0xffff);
	    vfb[3*j+2] = (l4_uint8_t) (fgc >> 16);
	 }
	 else {
	    *(l4_uint16_t*) (&vfb[3*j]) = (l4_uint16_t) (bgc & 0xffff);
	    vfb[3*j+2] = (l4_uint8_t) (bgc >> 16);
	 }
      }
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*3);
#endif
      vfb += bwidth;
      /* length of one line in bmap parsed */
      nobits += offset->endskip_x;
   }
}

static inline void
_bmap32lsb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
   l4_uint32_t nobits=0;
   l4_uint32_t i,j, k,kmod;

   nobits += offset->preskip_y
      * (w + offset->preskip_x + offset->endskip_x);
   /* length of one line in bmap (bits!) */

   for (i = 0; i < h; i++) {
      nobits += offset->preskip_x;
      for (j = 0; j < w; j++, nobits++) {
	 l4_uint32_t *dest = (l4_uint32_t*)&vfb[4*j];
	 k = nobits>>3;
	 kmod = (nobits)%8;
	 *dest = (bmap[k] & (0x01 << kmod))
	    ? fgc & 0xffffffff
	    : bgc & 0xffffffff;
      }
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*4);
#endif
      vfb += bwidth;
   }
}

static inline void
_bmap32msb(l4_uint8_t *vfb,
	   l4_uint8_t *bmap,
	   l4_uint32_t fgc,
	   l4_uint32_t bgc,
	   l4_uint32_t w, l4_uint32_t h,
	   struct gfxbitmap_offset* offset,
	   l4_uint32_t bwidth)
{
   l4_uint32_t nobits=0;
   l4_uint32_t i,j,k,kmod;

   nobits += offset->preskip_y
      * (w + offset->preskip_x + offset->endskip_x);
   /* length of one line in bmap (bits!) */

   for (i = 0; i < h; i++) {
      nobits += offset->preskip_x;
      for (j = 0; j < w; j++, nobits++) {
	 k = nobits>>3;
	 kmod = (nobits)%8;
	 if ( bmap[k] & (0x80 >> kmod) )
	    *(l4_uint32_t*) (&vfb[4*j]) = (l4_uint32_t) (fgc & 0x00ffffff);
	 else
	    *(l4_uint32_t*) (&vfb[4*j]) = (l4_uint32_t) (bgc & 0x00ffffff);
      }
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)vfb,
                               (unsigned long)vfb + w*4);
#endif
      vfb += bwidth;
      /* length of one line in bmap parsed */
      nobits += offset->endskip_x;
   }
}

static inline void
_set16(l4_uint8_t *dest,
       l4_uint8_t *pmap,
       l4_uint32_t w, l4_uint32_t h,
       struct gfxbitmap_offset* offset,
       l4_uint32_t bwidth,
       l4_uint32_t pwidth)
{
  l4_uint32_t i;

#ifdef ARCH_x86
  if (use_fastmemcpy && (w % 4 == 0))
    {
      asm ("emms");
      for (i = 0; i < h; i++)
	{
	  l4_umword_t dummy;
	  pmap += 2 * offset->preskip_x;
	  asm volatile("xorl    %%edx,%%edx              \n\t"
		       "1:                               \n\t"
	               "movq    (%%esi,%%edx,8),%%mm0    \n\t"
	               "movntq  %%mm0,(%%edi,%%edx,8)    \n\t"
		       "add     $1,%%edx                 \n\t"
		       "dec     %%ecx                    \n\t"
		       "jnz     1b                       \n\t"
		       : "=c"(dummy), "=d"(dummy)
		       : "c"(w/4), "S"(pmap), "D"(dest));
	  dest += bwidth;
	  pmap += pwidth;
	}
      asm ("sfence; emms");
    }
  else
#endif
    {
      for (i = 0; i < h; i++)
	{
	  pmap += 2 * offset->preskip_x;
	  memcpy(dest, pmap, w*2);
#ifdef CLEAN_CACHE
	  l4_sys_cache_clean_range((unsigned long)dest,
                                   (unsigned long)dest + w*2);
#endif
	  dest += bwidth;
	  pmap += pwidth;
	}
    }
}

static inline void
_set24(l4_uint8_t *dest,
       l4_uint8_t *pmap,
       l4_uint32_t w, l4_uint32_t h,
       struct gfxbitmap_offset* offset,
       l4_uint32_t bwidth,
       l4_uint32_t pwidth)
{
  l4_uint32_t i;

  for (i = 0; i < h; i++)
    {
      pmap += 3 * offset->preskip_x;
      memcpy(dest, pmap, w*3);
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)dest,
                               (unsigned long)dest + w*3);
#endif
      dest += bwidth;
      pmap += pwidth;
    }
}

static inline void
_set32(l4_uint8_t *dest,
       l4_uint8_t *pmap,
       l4_uint32_t w, l4_uint32_t h,
       struct gfxbitmap_offset* offset,
       l4_uint32_t bwidth,
       l4_uint32_t pwidth)
{
  l4_uint32_t i;

  for (i = 0; i < h; i++)
    {
      pmap += 4 * offset->preskip_x;
      memcpy(dest, pmap, w*4);
#ifdef CLEAN_CACHE
      l4_sys_cache_clean_range((unsigned long)dest,
                               (unsigned long)dest + w*4);
#endif
      dest += bwidth;
      pmap += pwidth;
   }
}

static inline void
_copy16(l4_uint8_t *dest,
        l4_uint8_t *src,
	l4_int16_t x, l4_int16_t y,
	l4_int16_t dx, l4_int16_t dy,
	l4_uint32_t w, l4_uint32_t h,
	l4_uint32_t bwidth)
{
  l4_uint32_t i;

  if (dy == y && dx == x)
    return;

  if (y >= dy)
    {
      OFFSET( x,  y, src,  2);
      OFFSET(dx, dy, dest, 2);
      for (i = 0; i < h; i++)
	{
	  /* memmove can deal with overlapping regions */
	  memmove(dest, src, 2*w);
	  src += bwidth;
	  dest += bwidth;
	}
    }
  else
    {
      OFFSET( x,  y + h - 1, src,  2);
      OFFSET(dx, dy + h - 1, dest, 2);
      for (i = 0; i < h; i++)
	{
	  /* memmove can deal with overlapping regions */
	  memmove(dest, src, 2*w);
	  src -= bwidth;
	  dest -= bwidth;
	}
    }
}

static inline void
_copy24(l4_uint8_t *dest,
        l4_uint8_t *src,
	l4_int16_t x, l4_int16_t y,
	l4_int16_t dx, l4_int16_t dy,
	l4_uint32_t w, l4_uint32_t h,
	l4_uint32_t bwidth)
{
   l4_uint32_t i;

   if (y >= dy) {
      if (y == dy && dx >= x) {	/* tricky */
         int j;
	 if (x == dx)
	    return;
	 /* my way: start right go left */
	 OFFSET( x,  y, src,  3);
	 OFFSET(dx, dy, dest, 3);
	 for (i = 0; i < h; i++) {
	    for (j = w; j >= 0; --j) {
	        *(l4_uint16_t*) (&dest[3*j]) = *(l4_uint16_t*) (&src[3*j]);
		dest[3*j+2] = src[3*j+2];
	    }
	    src += bwidth;
	    dest += bwidth;
	 }

      }
      else {		/* copy from top to bottom */
         l4_uint32_t j;
	 OFFSET( x,  y, src,  3);
	 OFFSET(dx, dy, dest, 3);
	 for (i = 0; i < h; i++) {
	    for (j = 0; j < w; j++) {
	       *(l4_uint16_t*) (&dest[3*j]) = *(l4_uint16_t*) (&src[3*j]);
	       dest[3*j+2] = src[3*j+2];
	    }
	    src += bwidth;
	    dest += bwidth;
	 }
      }
   }
   else {		/* copy from bottom to top */
      OFFSET( x,  y + h, src,  3);
      OFFSET(dx, dy + h, dest, 3);
      for (i = 0; i < h; i++) {
         l4_uint32_t j;
	 src -= bwidth;
	 dest -= bwidth;
	 for (j = 0; j < w; j++) {
	    *(l4_uint16_t*) (&dest[3*j]) = *(l4_uint16_t*) (&src[3*j]);
	    dest[3*j+2] = src[3*j+2];
	 }
      }
   }
}

static inline void
_copy32(l4_uint8_t *dest,
        l4_uint8_t *src,
        l4_int16_t x, l4_int16_t y,
        l4_int16_t dx, l4_int16_t dy,
        l4_uint32_t w, l4_uint32_t h,
        l4_uint32_t bwidth)
{
   l4_uint32_t i;

   if (y >= dy) {
      if (y == dy && dx >= x) {	/* tricky */
	 int j;
	 if (x == dx)
	    return;
	 /* my way: start right go left */
	 OFFSET( x,  y, src,  4);
	 OFFSET(dx, dy, dest, 4);
	 for (i = 0; i < h; i++) {
	    for (j = w; j >= 0; --j)
	       *(l4_uint32_t*) (&dest[4*j]) = *(l4_uint32_t*) (&src[4*j]);
	    src += bwidth;
	    dest += bwidth;
	 }

      }
      else {		/* copy from top to bottom */
	 l4_uint32_t j;
	 OFFSET( x,  y, src,  4);
	 OFFSET(dx, dy, dest, 4);
	 for (i = 0; i < h; i++) {
	    for (j = 0; j < w; j++)
	       *(l4_uint32_t*) (&dest[4*j]) = *(l4_uint32_t*) (&src[4*j]);
	    src += bwidth;
	    dest += bwidth;
	 }
      }
   }
   else {		/* copy from bottom to top */
      l4_uint32_t j;
      OFFSET( x,  y + h, src,  4);
      OFFSET(dx, dy + h, dest, 4);
      for (i = 0; i < h; i++) {
	 src -= bwidth;
	 dest -= bwidth;
	 for (j = 0; j < w; j++)
	    *(l4_uint32_t*) (&dest[4*j]) = *(l4_uint32_t*) (&src[4*j]);
      }
   }
}

static inline void
_fill16(l4_uint8_t *vfb,
	l4_uint32_t w, l4_uint32_t h,
	l4_uint32_t color,
	l4_uint32_t bwidth)
{
  l4_uint32_t i,j;

  for (i = 0; i < h; i++)
    {
      for (j = 0; j < w; j++)
	*(l4_uint16_t*) (&vfb[2*j]) = (l4_uint16_t)color;
      vfb += bwidth;
    }
}

static inline void
_fill24(l4_uint8_t *vfb,
	l4_uint32_t w, l4_uint32_t h,
	l4_uint32_t color,
	l4_uint32_t bwidth)
{
   l4_uint32_t i,j;

   for (i = 0; i < h; i++) {
      for (j = 0; j < w; j++) {
	 *(l4_uint16_t*) (&vfb[3*j  ]) = (l4_uint16_t)color;
	                   vfb[3*j+2]  = (l4_uint8_t) (color >> 16);
      }
      vfb += bwidth;
   }
}

static inline void
_fill32(l4_uint8_t *vfb,
	l4_uint32_t w, l4_uint32_t h,
	l4_uint32_t color,
	l4_uint32_t bwidth)
{
   l4_uint32_t i,j;

   for (i = 0; i < h; i++) {
      for (j = 0; j < w; j++)
	 *(l4_uint32_t*) (&vfb[4*j]) = (l4_uint32_t)color;
      vfb += bwidth;
   }
}

void
gfxbitmap_fill(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
               int x, int y, int w, int h, unsigned color)
{
  unsigned bwidth = vi->bytes_per_line;
  OFFSET(x, y, vfb, vi->pixel_info.bytes_per_pixel);

  switch (vi->pixel_info.bytes_per_pixel)
    {
    case 4:
      _fill32(vfb, w, h, color, vi->bytes_per_line);
      break;
    case 3:
      _fill24(vfb, w, h, color, vi->bytes_per_line);
      break;
    case 2:
    default:
      _fill16(vfb, w, h, color, vi->bytes_per_line);
    }
}

void
gfxbitmap_bmap(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
               l4_int16_t x, l4_int16_t y, l4_uint32_t w,
               l4_uint32_t h, l4_uint8_t *bmap, l4_uint32_t fgc, l4_uint32_t bgc,
               struct gfxbitmap_offset* offset, l4_uint8_t mode)
{
  l4_uint32_t bwidth = vi->bytes_per_line;
  OFFSET(x, y, vfb, vi->pixel_info.bytes_per_pixel);

  switch (mode)
    {
    case pSLIM_BMAP_START_MSB:
      switch (vi->pixel_info.bytes_per_pixel)
        {
        case 4:
          _bmap32msb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
          break;
        case 3:
          _bmap24msb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
          break;
        case 2:
        default:
          _bmap16msb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
        }
      break;
    case pSLIM_BMAP_START_LSB:
    default:	/* `start at least significant' bit is default */
      switch (vi->pixel_info.bytes_per_pixel)
        {
        case 4:
          _bmap32lsb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
          break;
        case 3:
          _bmap24lsb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
          break;
        case 2:
        default:
          _bmap16lsb(vfb, bmap, fgc, bgc, w, h, offset, bwidth);
        }
    }
}

void
gfxbitmap_set(l4_uint8_t *vfb, l4re_video_view_info_t *vi,
              l4_int16_t x, l4_int16_t y, l4_uint32_t w,
              l4_uint32_t h, l4_uint32_t xoffs, l4_uint32_t yoffs,
              l4_uint8_t *pmap, struct gfxbitmap_offset* offset,
              l4_uint32_t pwidth)
{
  l4_uint32_t bwidth = vi->bytes_per_line;

  OFFSET(x+xoffs, y+yoffs, vfb, vi->pixel_info.bytes_per_pixel);

  switch (vi->pixel_info.bytes_per_pixel)
    {
    case 4:
      _set32(vfb, pmap, w, h, offset, bwidth, pwidth);
      break;
    case 3:
      _set24(vfb, pmap, w, h, offset, bwidth, pwidth);
      break;
    case 2:
    default:
      _set16(vfb, pmap, w, h, offset, bwidth, pwidth);
    }
}

void
gfxbitmap_copy(l4_uint8_t *dest, l4_uint8_t *src, l4re_video_view_info_t *vi,
               int x, int y, int w, int h, int dx, int dy)
{
  switch (vi->pixel_info.bytes_per_pixel)
    {
    case 4:
      _copy32(dest, src, x, y, dx, dy, w, h, vi->bytes_per_line);
      break;
    case 3:
      _copy24(dest, src, x, y, dx, dy, w, h, vi->bytes_per_line);
      break;
    case 2:
    default:
      _copy16(dest, src, x, y, dx, dy, w, h, vi->bytes_per_line);
    }
}

gfxbitmap_color_pix_t
gfxbitmap_convert_color(l4re_video_view_info_t *vi, gfxbitmap_color_t rgb)
{
  switch (l4re_video_bits_per_pixel(&vi->pixel_info))
  {
    case 24:
    case 32:
      return rgb & 0x00FFFFFF;

    case 15:
      return (((rgb >> (16 + 8 - vi->pixel_info.r.size)) & ((1 << vi->pixel_info.r.size)-1)) << vi->pixel_info.r.shift)
           | (((rgb >> ( 8 + 8 - vi->pixel_info.g.size)) & ((1 << vi->pixel_info.g.size)-1)) << vi->pixel_info.g.shift)
           | (((rgb >> ( 0 + 8 - vi->pixel_info.b.size)) & ((1 << vi->pixel_info.b.size)-1)) << vi->pixel_info.b.shift);

    case 16:
    default:
      return ((rgb & 0x00F80000) >> 8)
           | ((rgb & 0x0000FC00) >> 5)
           | ((rgb & 0x000000F8) >> 3);
  }

}
