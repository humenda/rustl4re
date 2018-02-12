
/**
 * \file	con/server/src/pslim.c
 * \brief	implementation of pSLIM protocol
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdlib.h>
#include <string.h>		/* needed for memmove */

#include <l4/sys/cache.h>
#include <l4/libgfxbitmap/bitmap.h>

/* local includes */
#include "main.h"
#include "l4con.h"
#include "pslim.h"
#include "con_hw/init.h"
#include "con_yuv2rgb/yuv2rgb.h"

/* private types */
/** offsets in pmap[] and bmap[] */
struct pslim_offset
{
  l4_uint32_t preskip_x;	/**< skip pixels at beginning of line */
  l4_uint32_t preskip_y;	/**< skip lines */
  l4_uint32_t endskip_x;	/**< skip pixels at end of line */
/* word_t endskip_y; */	/* snip lines */
};

/* all pslim_*-functions call the vc->g_mode specific functions */
static inline void sw_bmap(struct l4con_vc*, l4_int16_t, l4_int16_t,
			   l4_uint32_t, l4_uint32_t,
			   l4_uint8_t *bmap, l4_uint32_t fgc, l4_uint32_t bgc,
			   struct pslim_offset*, l4_uint8_t mode);
static inline void  sw_set(struct l4con_vc*, l4_int16_t, l4_int16_t,
			   l4_uint32_t, l4_uint32_t, l4_uint32_t, l4_uint32_t,
			   l4_uint8_t *pmap, struct pslim_offset*);
static inline void sw_cscs(struct l4con_vc*, l4_int16_t, l4_int16_t,
			   l4_uint32_t, l4_uint32_t,
			   l4_uint8_t *y, l4_uint8_t *u, l4_uint8_t *v,
			   l4_uint32_t scale, struct pslim_offset*,
			   l4_uint8_t mode);

/* clipping */

static inline int
clip_rect(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect)
{
  int max_x = vc->xres;
  int max_y = vc->yres;

  if (from_user)
    {
      rect->x += vc->client_xofs;
      rect->y += vc->client_yofs;
      max_x    = vc->client_xres;
      max_y    = vc->client_yres;
    }

  if ((rect->x > max_x) || (rect->y > max_y))
    /* not in the frame buffer */
    return 0;
  if (rect->x < 0)
    {
      if (-rect->x >= rect->w)
	/* not visible - left of border */
	return 0;
      /* clip left */
      rect->w += rect->x;
      rect->x  = 0;
    }
  if (rect->y < 0)
    {
      if (-rect->y >= rect->h)
	/* not visible - above border */
	return 0;
      /* clip top */
      rect->h += rect->y;
      rect->y  = 0;
    }
  if ((rect->x + rect->w) > max_x)
    /* clip right */
    rect->w = max_x - rect->x;
  if ((rect->y + rect->h) > max_y)
    /* clip bottom */
    rect->h = max_y - rect->y;

  /* something is visible */
  return 1;
}

static inline int
clip_rect_offset(struct l4con_vc *vc, int from_user,
		 l4con_pslim_rect_t *rect, struct pslim_offset *offset)
{
  int max_x = vc->xres;
  int max_y = vc->yres;

  if (from_user)
    {
      rect->x += vc->client_xofs;
      rect->y += vc->client_yofs;
      max_x    = vc->client_xres;
      max_y    = vc->client_yres;
    }

  if ((rect->x > max_x) || (rect->y > max_y))
    /* not in the frame buffer */
    return 0;
  if (rect->x < 0)
    {
      if (-rect->x > rect->w)
	/* not visible - left of border */
	return 0;
      /* clip left */
      rect->w           += rect->x;
      offset->preskip_x  = -rect->x;
      rect->x            = 0;
    }
  if (rect->y < 0)
    {
      if (-rect->y > rect->h)
	/* not visible - above border */
	return 0;
      /* clip top */
      rect->h           += rect->y;
      offset->preskip_y  = -rect->y;
      rect->y            = 0;
    }
  if ((rect->x + rect->w) > max_x)
    {
      /* clip right */
      offset->endskip_x  = rect->x + rect->w - max_x;
      rect->w = max_x - rect->x;
    }
  if ((rect->y + rect->h) > max_y)
    /* clip bottom */
    rect->h = max_y - rect->y;

  /* something is visible */
  return 1;
}

static inline int
clip_rect_dxy(struct l4con_vc *vc, int from_user,
	      l4con_pslim_rect_t *rect, l4_int16_t *dx, l4_int16_t *dy)
{
  int max_x = vc->xres;
  int max_y = vc->yres;

  if (from_user)
    {
      rect->x += vc->client_xofs;
      rect->y += vc->client_yofs;
      max_x    = vc->client_xres;
      max_y    = vc->client_yres;
    }

  /* clip source rectangle */
  if ((rect->x > max_x) || (rect->y > max_y))
    /* not in the frame buffer */
    return 0;
  if (rect->x < 0)
    {
      if (-rect->x > rect->w)
	/* not visible - left of border */
	return 0;
      /* clip left */
      rect->w += rect->x;
      *dx     -= rect->x;
      rect->x  = 0;
    }
  if (rect->y < 0)
    {
      if (-rect->y > rect->h)
	/* not visible - above border */
	return 0;
      /* clip top */
      rect->h += rect->y;
      *dy     -= rect->y;
      rect->y  = 0;
    }
  if ((rect->x + rect->w) > max_x)
    /* clip right */
    rect->w = max_x - rect->x;
  if ((rect->y + rect->h) > max_y)
    /* clip bottom */
    rect->h = max_y - rect->y;

  /* clip destination rectangle */
  if ((*dx > max_x) || (*dy > max_y))
    /* not in the frame buffer */
    return 0;
  if (*dx < 0)
    {
      if (-*dx > rect->w)
	/* not visible - left of border */
	return 0;
      /* clip left */
      rect->w += *dx;
      rect->x -= *dx;
      *dx = 0;
    }
  if (*dy < 0)
    {
      if (-*dy > rect->h)
	/* not visible - above border */
	return 0;
      /* clip top */
      rect->h += *dy;
      rect->y -= *dy;
      *dy = 0;
    }
  if ((*dx + rect->w) > max_x)
    /* clip right */
    rect->w = max_x - *dx;
  if ((*dy + rect->h) > max_y)
    /* clip bottom */
    rect->h = max_y - *dy;

  /* something is visible */
  return 1;
}

void
sw_fill(struct l4con_vc *vc, int x, int y, int w, int h, unsigned color)
{
  l4_uint8_t *vfb = (l4_uint8_t*) vc->fb;

  /* wait for any pending acceleration operation */
  vc->do_sync();

  {
    l4re_video_view_info_t vvi;
    vvi.bytes_per_line              = vc->bytes_per_line;
    vvi.pixel_info.bytes_per_pixel  = vc->bytes_per_pixel;
    vvi.pixel_info.r.size           = vc->bpp;
    gfxbitmap_fill(vfb, &vvi, x, y, w, h, color);
  }

  /* force redraw of changed screen content (needed by VMware) */
  if (vc->do_drty)
    vc->do_drty(x, y, w, h);
}

static inline void
sw_bmap(struct l4con_vc *vc, l4_int16_t x, l4_int16_t y, l4_uint32_t w,
	l4_uint32_t h, l4_uint8_t *bmap, l4_uint32_t fgc, l4_uint32_t bgc,
        struct pslim_offset* offset, l4_uint8_t mode)
{
  l4_uint8_t *vfb = (l4_uint8_t*) vc->fb;
  l4re_video_view_info_t vvi;

  /* wait for any pending acceleration operation */
  vc->do_sync();

  vvi.bytes_per_line              = vc->bytes_per_line;
  vvi.pixel_info.bytes_per_pixel  = vc->bytes_per_pixel;
  vvi.pixel_info.r.size           = vc->bpp;
  gfxbitmap_bmap(vfb, &vvi, x, y, w, h, bmap, fgc, bgc,
                 (struct gfxbitmap_offset *)offset, mode);

  /* force redraw of changed screen content (needed by VMware) */
  if (vc->do_drty)
    vc->do_drty(x, y, w, h);
}

static inline void
sw_set(struct l4con_vc *vc, l4_int16_t x, l4_int16_t y, l4_uint32_t w,
       l4_uint32_t h, l4_uint32_t xoffs, l4_uint32_t yoffs,
       l4_uint8_t *pmap, struct pslim_offset* offset)
{
  l4_uint8_t *vfb = (l4_uint8_t*) vc->fb;
  l4_uint32_t bytepp = vc->bytes_per_pixel;
  l4_uint32_t pwidth;
  l4re_video_view_info_t vvi;

  if (!pmap)
    {
      /* copy from direct mapped framebuffer of client */
      /* bwidth may be different from xres*bytepp: e.g. VMware */
      pwidth = vc->bytes_per_line;
      pmap   = vc->vfb + y*pwidth + x*bytepp;
    }
  else
    {
      pwidth = bytepp * (w + offset->endskip_x);
      pmap  += bytepp * offset->preskip_y *
		        (w + offset->preskip_x + offset->endskip_x);
    }

  /* wait for any pending acceleration operation */
  vc->do_sync();

  vvi.bytes_per_line              = vc->bytes_per_line;
  vvi.pixel_info.bytes_per_pixel  = vc->bytes_per_pixel;
  vvi.pixel_info.r.size           = vc->bpp;
  gfxbitmap_set(vfb, &vvi, x, y, w, h, xoffs, yoffs, pmap,
                (struct gfxbitmap_offset *)offset, pwidth);

  /* force redraw of changed screen content (needed by VMware) */
  if (vc->do_drty)
    vc->do_drty(x+xoffs, y+yoffs, w, h);
}

void
sw_copy(struct l4con_vc *vc, int x, int y, int w, int h, int dx, int dy)
{
  l4_uint8_t *vfb = (l4_uint8_t*) vc->fb;
  l4re_video_view_info_t vvi;

  /* wait for any pending acceleration operation */
  vc->do_sync();

  vvi.bytes_per_line              = vc->bytes_per_line;
  vvi.pixel_info.bytes_per_pixel  = vc->bytes_per_pixel;
  vvi.pixel_info.r.size           = vc->bpp;
  gfxbitmap_copy(vfb, vfb, &vvi, x, y, w, h, dx, dy);

  /* force redraw of changed screen content (needed by VMware) */
  if (vc->do_drty)
    vc->do_drty(dx, dy, w, h);
}

static inline void
sw_cscs(struct l4con_vc *vc, l4_int16_t x, l4_int16_t y, l4_uint32_t w,
	l4_uint32_t h, l4_uint8_t *Y, l4_uint8_t *U, l4_uint8_t *V,
	l4_uint32_t scale, struct pslim_offset* offset, l4_uint8_t mode)
{
  (void)scale; (void)offset; (void)mode;
  /* wait for any pending acceleration operation */
  vc->do_sync();

  /* we exchange U and V here because of video format 420 */
  (*yuv2rgb_render)(vc->fb+y*vc->bytes_per_line+x*vc->bytes_per_pixel,
		    Y, U, V, w, h, vc->bytes_per_line, w, w/2);

  /* force redraw of changed screen content (needed by VMware) */
  if (vc->do_drty)
    vc->do_drty(x, y, w, h);
}

/* SVGAlib calls this: FILLBOX */
void
pslim_fill(struct l4con_vc *vc, int from_user,
	   l4con_pslim_rect_t *rect, l4con_pslim_color_t color)
{
  if (!clip_rect(vc, from_user, rect))
    /* nothing visible */
    return;

  if (!rect->w || !rect->h)
    /* nothing todo */
    return;

  vc->do_fill(vc, rect->x+vc->pan_xofs, rect->y+vc->pan_yofs,
                  rect->w, rect->h, color);
}

/* SVGAlib calls this:	ACCEL_PUTBITMAP (mode 2)
			PBM - images (mode 1)		*/
void
pslim_bmap(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
	   l4con_pslim_color_t fgc, l4con_pslim_color_t bgc, void* bmap,
	   l4_uint8_t mode)
{
  struct pslim_offset offset = {0,0,0};

  if (!clip_rect_offset(vc, from_user, rect, &offset))
    /* nothing visible */
    return;

  if (!rect->w || !rect->h)
    /* nothing todo */
    return;

  sw_bmap(vc, rect->x+vc->pan_xofs, rect->y+vc->pan_yofs, rect->w, rect->h,
              bmap, fgc, bgc, &offset, mode);
}


/* SVGAlib calls this:	PUTBOX  (and knows about masked boxes!) */
void
pslim_set(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
	  void* pmap)
{
  struct pslim_offset offset = {0,0,0};

  if (!clip_rect_offset(vc, from_user, rect, &offset))
    /* nothing visible */
    return;

  if (!rect->w || !rect->h)
    /* nothing todo */
    return;

  sw_set(vc, rect->x, rect->y, rect->w, rect->h,
             vc->pan_xofs, vc->pan_yofs, (l4_uint8_t*) pmap, &offset);
}

/* SVGAlib calls this:	COPYBOX */
void
pslim_copy(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
	   l4_int16_t dx, l4_int16_t dy)
{
  if (!clip_rect_dxy(vc, from_user, rect, &dx, &dy))
    /* nothing visible */
    return;

  if (!rect->w || !rect->h)
    /* nothing todo */
    return;

  vc->do_copy(vc, rect->x+vc->pan_xofs, rect->y+vc->pan_yofs, rect->w, rect->h,
                  dx+vc->pan_xofs, dy+vc->pan_yofs);
}

/* COLOR-SPACE CONVERT and (optional) SCALE
 *
 * mode ... Specifies the resolution of chrominance data
 *
 *	Code     Meaning
 *	----     ------- ----------------------------------------
 *	PLN_420  4:2:0   half resolution in both dimensions (most common format)
 *	PLN_422  4:2:2   half resolution in horizontal direction
 *	PLN_444  4:4:4   full resolution
 */
void
pslim_cscs(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
	   void* y, void* u, void *v, l4_uint8_t mode, l4_uint32_t scale)
{
  struct pslim_offset offset = {0,0,0};

  if (!clip_rect_offset(vc, from_user, rect, &offset))
    /* nothing visible */
    return;

  if (!rect->w || !rect->h)
    /* nothing todo */
    return;

  sw_cscs(vc, rect->x+vc->pan_xofs, rect->y+vc->pan_yofs, rect->w, rect->h,
          y, u, v, scale, &offset, mode);
}
