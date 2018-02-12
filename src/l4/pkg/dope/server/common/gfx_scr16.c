/*
 * \brief  DOpE gfx 16bit screen handler module
 * \date   2003-04-02
 * \author Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "scrdrv.h"
#include "cache.h"
#include "fontman.h"
#include "clipping.h"
#include "gfx.h"
#include "gfx_handler.h"

#define RGBA_TO_RGB16(c) (((c&0xf8000000)>>16)|((c&0x00fc0000)>>13)|((c&0x0000f800)>>11))
#define COL16(r, g, b) (r<<11) + (g<<6) + b

static struct scrdrv_services   *scrdrv;
static struct fontman_services  *fontman;
static struct clipping_services *clip;
static struct cache_services    *cache;

static CACHE *imgcache = NULL;

static s32  clip_x1, clip_y1, clip_x2, clip_y2;
static u16 *scr_adr;
static s32  scr_width, scr_height, scr_type;

int init_gfxscr16(struct dope_services *d);

#ifdef GFX_ASM

/*** DRAW SOLID BOX ***/
extern void asm_solid_fill_16(u16 *dst, long line_width,
                              long width, long height, long color);

/*** DRAW TRANSPARENT BOX ***/
extern void asm_solid_mixed_16(u16 *dst, long line_width,
                               long width, long height, long color);

/*** DRAW SCALED IMAGE ***/
extern void asm_scaled_img_16(u16 *src, u8 *dst, long sx, long sy,
                              long mx, long my, long src_w, long dst_w,
                              long w, long h);

#endif


/*************************
 *** PRIVATE FUNCTIONS ***
 *************************/

/*** BLEND 16BIT COLOR WITH SPECIFIED ALPHA VALUE ***/
static inline u16 blend(u16 color, int alpha) {
	return ((((alpha >> 3) * (color & 0xf81f)) >> 5) & 0xf81f)
	      | (((alpha * (color & 0x07e0)) >> 8) & 0x7e0);
}


/*** DRAW A SOLID HORIZONTAL LINE IN 16BIT COLOR MODE ***/
static inline void solid_hline_16(u16 *dst, u32 width, u16 col) {
	for (;width--;dst++) *dst = col;
}


/*** DRAW A SOLID VERTICAL LINE IN 16BIT COLOR MODE ***/
static inline void solid_vline_16(u16 *dst, u32 height, u32 scr_w, u16 col) {
	for (;height--;dst+=scr_w) *dst = col;
}


/*** DRAW A TRANSPARENT HORIZONTAL LINE IN 16BIT COLOR MODE ***/
static inline void mixed_hline_16(u16 *dst, u32 width, u16 mixcol) {
	mixcol=(mixcol&0xf7de)>>1;
	for (;width--;dst++) *dst = (((*dst)&0xf7de)>>1) + mixcol;
}


/*** DRAW A TRANSPARENT VERTICAL LINE IN 16BIT COLOR MODE ***/
static inline void mixed_vline_16(u16 *dst, u32 height, u32 scr_w, u16 mixcol) {
	mixcol=(mixcol&0xf7de)>>1;
	for (;height--;dst+=scr_w) *dst = (((*dst)&0xf7de)>>1) + mixcol;
}


/*** CLIP IMAGE AGAINST CLIPPING AREA ***/
static inline int clip_img(int clip_x1, int clip_y1, int clip_x2, int clip_y2,
                           int *x,  int *y,  int *w, int *h,
                           int *sx, int *sy, int mx, int my) {

	/* left clipping */
	if (*x   <  clip_x1) {
		*w  -=  clip_x1 - *x;
		*sx += (clip_x1 - *x) * mx;
		*x   =  clip_x1;
	}

	/* right clipping */
	if (*w > clip_x2 - *x + 1)
		*w = clip_x2 - *x + 1;

	/* top clipping */
	if (*y   <  clip_y1) {
		*h  -=  clip_y1 - *y;
		*sy += (clip_y1 - *y) * my;
		*y   =  clip_y1;
	}

	/* bottom clipping */
	if (*h > clip_y2 - *y + 1)
		*h = clip_y2 - *y + 1;

	return ((*w >= 0) && (*h >= 0));
}


/*** DRAW CLIPPED 16BIT IMAGE TO 16BIT SCREEN ***/
static inline void paint_img_16(int x, int y, int img_w, int img_h, u16 *src) {
	int i, j;
	int w = img_w, h = img_h;
	u16 *dst, *s, *d;
	int sx = 0, sy = 0;

	if (!clip_img(clip_x1, clip_y1, clip_x2, clip_y2,
	              &x, &y, &w, &h, &sx, &sy, 1, 1)) return;

	/* calculate start address */
	src += img_w*sy + sx;
	dst  = scr_adr + y*scr_width + x;

	/* paint... */
	for (j = h; j--; ) {

		/* copy line from image to screen */
		for (i = w, s = src, d = dst; i--; *(d++) = *(s++));
		src += img_w;
		dst += scr_width;
	}
}


static s32 scale_xbuf[2000];

/*** DRAW SCALED AND CLIPPED 16BIT IMAGE TO 16BIT SCREEN ***/
static void paint_scaled_img_rgb16(int x, int y, int w, int h,
                                   int linewidth, int sw, int sh, u16 *src) {
	int mx, my;
	int i, j;
	int sx = 0, sy = 0;
	u16 *dst, *s, *d;

	/* sanity check */
	if (!src) return;

//	/* use shortcut for non-scaled images */
//	if ((w == iw) && (h == ih)) {
//		paint_img_16(x, y, img_w, img_h, src);
//		return;
//	}

	mx = w ? ((long)sw<<16) / w : 0;
	my = h ? ((long)sh<<16) / h : 0;

	if (!clip_img(clip_x1, clip_y1, clip_x2, clip_y2,
	              &x, &y, &w, &h, &sx, &sy, mx, my)) return;

	/* calculate start address */
	dst = scr_adr + y*scr_width + x;

	/* calculate x offsets */
	for (i = w; i--; sx += mx)
		scale_xbuf[i] = sx >> 16;

	/* draw scaled image */
	for (j = h; j--; sy += my, dst += scr_width) {
		s = src + ((sy>>16)*linewidth);
		d = dst;
		for (i = w; i--; )
			*(d++) = *(s + scale_xbuf[i]);
	}
}


/*** DRAW SCALED AND CLIPPED 32BIT ARGB IMAGE TO 16BIT SCREEN ***/
static void paint_scaled_img_rgba32(int x, int y, int w, int h,
                                    int linewidth, int sw, int sh, u32 *src) {
	int mx, my;
	int i, j;
	int sx = 0, sy = 0;
	u16 *dst, *d;
	u32 *s;

	/* sanity check */
	if (!src) return;

	mx = w ? ((long)sw<<16) / w : 0;
	my = h ? ((long)sh<<16) / h : 0;

	if (!clip_img(clip_x1, clip_y1, clip_x2, clip_y2,
	              &x, &y, &w, &h, &sx, &sy, mx, my)) return;

	/* calculate start address */
	dst = scr_adr + y*scr_width + x;

	/* calculate x offsets */
	for (i = w; i--; sx += mx)
		scale_xbuf[i] = sx >> 16;

	/* draw scaled image */
	for (j = h; j--; sy += my, dst += scr_width) {
		s = src + ((sy>>16)*linewidth);
		d = dst;
		for (i = w; i--; d++) {
			u32 color = *(s + scale_xbuf[i]);
			int alpha = GFX_A(color);
			if (alpha) *d = blend(*d, 255 - alpha) + blend(RGBA_TO_RGB16(color), alpha);
		}
	}
}

/*** CONVERT A YUV420 IMAGE TO A 16-BIT HICOLOR IMAGE ***/

static const s32 Inverse_Table_6_9[8][4] = {
	{117504, 138453, 13954, 34903}, /* no sequence_display_extension */
	{117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
	{104597, 132201, 25675, 53279}, /* unspecified */
	{104597, 132201, 25675, 53279}, /* reserved */
	{104448, 132798, 24759, 53109}, /* FCC */
	{104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
	{104597, 132201, 25675, 53279}, /* SMPTE 170M */
	{117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};

static u16 tab_16[197 + 2*682 + 256 + 132];
static void *table_rV[256];
static void *table_gU[256];
static int   table_gV[256];
static void *table_bU[256];

#define RGB(i)\
U = pu_[i];\
	V = pv_[i];\
	r = table_rV[V];\
	g = (void *) (((u8 *)table_gU[U]) + table_gV[V]);\
	b = table_bU[U];

#define DST1(i)\
Y = py_1[2*i];\
	dst_1[2*i] = r[Y] + g[Y] + b[Y];\
	Y = py_1[2*i+1];\
	dst_1[2*i+1] = r[Y] + g[Y] + b[Y];

#define DST2(i)\
Y = py_2[2*i];\
	dst_2[2*i] = r[Y] + g[Y] + b[Y];\
	Y = py_2[2*i+1];\
	dst_2[2*i+1] = r[Y] + g[Y] + b[Y];

/* This is exactly the same code as yuv2rgb_c_32 except for the types of */
/* r, g, b, dst_1, dst_2 */

static void convert_yuv420_to_16(int width, int height, u8 *src_yuv420, u16 *dst_rgb16) {
	void *dst =dst_rgb16;
	u8 *py = src_yuv420;
	u8 *pu = src_yuv420 + width*height;
	u8 *pv = src_yuv420 + width*height + width*height/4;
	int rgb_stride = width*2;
	int y_stride = width;
	int uv_stride = width/2;
	int U, V, Y;
	u16 * r, * g, * b;
	u16 * dst_1, * dst_2;

	height >>= 1;
	do {

		u8 * py_1 = py;
		u8 * py_2 = py + y_stride;
		u8 * pu_  = pu;
		u8 * pv_  = pv;
		void * _dst_1 = dst;
		void * _dst_2 = ((u8 *)dst) + rgb_stride;
		int width_ = width;

		width_ >>= 3;
		dst_1 = _dst_1;
		dst_2 = _dst_2;

		do {
			RGB(0); DST1(0); DST2(0);
			RGB(1); DST2(1); DST1(1);
			RGB(2); DST1(2); DST2(2);
			RGB(3); DST2(3); DST1(3);

			pu_ += 4;
			pv_ += 4;
			py_1 += 8;
			py_2 += 8;
			dst_1 += 8;
			dst_2 += 8;
		} while (--width_);

		py += 2 * y_stride;
		pu += uv_stride;
		pv += uv_stride;
		dst = ((u8 *)dst) + 2 * rgb_stride;
	} while (--height);
}


static int div_r(int dividend, int divisor) {
	if (dividend > 0)
		return (dividend + (divisor>>1)) / divisor;
	else
		return -((-dividend + (divisor>>1)) / divisor);
}


static int init_yuv2rgb(void) {
	int i;
	u8 table_Y[1024];
	u16 * table_16 = &tab_16[0];
	void *table_r = 0, *table_g = 0, *table_b = 0;

	int crv =  Inverse_Table_6_9[6][0];
	int cbu =  Inverse_Table_6_9[6][1];
	int cgu = -Inverse_Table_6_9[6][2];
	int cgv = -Inverse_Table_6_9[6][3];

	for (i = 0; i < 1024; i++) {
		int j;
		j = (76309 * (i - 384 - 16) + 32768) >> 16;
		j = (j < 0) ? 0 : ((j > 255) ? 255 : j);
		table_Y[i] = j;
	}

	table_r = table_16 + 197;
	table_b = table_16 + 197 + 685;
	table_g = table_16 + 197 + 2*682;

	for (i = -197; i < 256+197; i++)
		((u16 *)table_r)[i] = (table_Y[i + 384] >> 3) << 11;

	for (i = -132; i < 256+132; i++)
		((u16 *)table_g)[i] = (table_Y[i + 384] >> 2) << 5;

	for (i = -232; i < 256+232; i++)
		((u16 *)table_b)[i] = (table_Y[i + 384] >> 3);

	for (i = 0; i < 256; i++) {
		table_rV[i] = (((u8 *)table_r) + 2 * div_r (crv * (i - 128), 76309));
		table_gU[i] = (((u8 *)table_g) + 2 * div_r (cgu * (i - 128), 76309));
		table_gV[i] = 2 * div_r (cgv * (i-128), 76309);
		table_bU[i] = (((u8 *)table_b) + 2 * div_r (cbu * (i - 128), 76309));
	}
	return 0;
}


/*****************************
 *** GFX HANDLER FUNCTIONS ***
 *****************************/

static s32 scr_get_width(struct gfx_ds_data *s) {
	return scr_width;
}


static s32 scr_get_height(struct gfx_ds_data *s) {
	return scr_height;
}


static s32 scr_get_type(struct gfx_ds_data *s) {
	return GFX_IMG_TYPE_RGB16;
}


static void scr_destroy(struct gfx_ds_data *s) {
	scrdrv->restore_screen();
}


static void *scr_map(struct gfx_ds_data *s) {
	return scr_adr;
}


static void scr_update(struct gfx_ds_data *s, s32 x, s32 y, s32 w, s32 h) {
	scrdrv->update_area(x, y, x + w - 1, y + h - 1);
}


static s32 scr_draw_hline_16(struct gfx_ds_data *s, s16 x, s16 y, s16 w, u32 rgba) {
	int beg_x, end_x;

	if (clip_y1 > y || clip_y2 < y) return 0;
	beg_x = MAX(x, clip_x1);
	end_x = MIN(x + w - 1, clip_x2);

	if (beg_x > end_x) return 0;

	if (GFX_A(rgba) > 127) {
		solid_hline_16(scr_adr + y*scr_width + beg_x, end_x - beg_x + 1, RGBA_TO_RGB16(rgba));
	} else {
		mixed_hline_16(scr_adr + y*scr_width + beg_x, end_x - beg_x + 1, RGBA_TO_RGB16(rgba));
	}
	return 0;
}


static s32 scr_draw_vline_16(struct gfx_ds_data *s, s16 x, s16 y, s16 h, u32 rgba) {
	int beg_y, end_y;

	if (clip_x1 > x || clip_x2 < x) return 0;
	beg_y = MAX(y, clip_y1);
	end_y = MIN(y + h - 1, clip_y2);
	if (beg_y > end_y) return 0;

	if (GFX_A(rgba) > 127) {
		solid_vline_16(scr_adr + beg_y*scr_width + x, end_y - beg_y + 1, scr_width, RGBA_TO_RGB16(rgba));
	} else {
		mixed_vline_16(scr_adr + beg_y*scr_width + x, end_y - beg_y + 1, scr_width, RGBA_TO_RGB16(rgba));
	}
	return 0;
}


static s32 scr_draw_fill_16(struct gfx_ds_data *s, s16 x1, s16 y1, s16 w, s16 h, u32 rgba) {
	static s16 x, y;
	static u16 *dst, *dst_line;
	static u16 color;
	static u16 alpha;
	s16 x2 = x1 + w - 1;
	s16 y2 = y1 + h - 1;

	/* check clipping */
	if (x1 < clip_x1) x1 = clip_x1;
	if (y1 < clip_y1) y1 = clip_y1;
	if (x2 > clip_x2) x2 = clip_x2;
	if (y2 > clip_y2) y2 = clip_y2;
	if (x1 > x2) return -1;
	if (y1 > y2) return -1;

	color = RGBA_TO_RGB16(rgba);
	alpha = GFX_A(rgba);

#ifdef GFX_ASM

	/* solid fill */
	if (alpha & 0x80)
		asm_solid_fill_16(scr_adr + scr_width*y1 + x1, scr_width, x2 - x1 + 1, y2 - y1 + 1, color);

	/* mix colors for alpha mode */
	else
		asm_mixed_fill_16(scr_adr + scr_width*y1 + x1, scr_width, x2 - x1 + 1, y2 - y1 + 1, color);

#else

	dst_line = scr_adr + scr_width*y1;

	/* solid fill for 100% alpha */
	if (alpha == 0xff) {
		for (y = y1; y <= y2; y++, dst_line += scr_width)
			for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
				*dst = color;

	/* mix colors for 50% alpha */
	} else if (alpha == 0x7f) {
		color = (color >> 1) & 0x7bef;    /* 50% alpha mode */
		for (y = y1; y <= y2; y++, dst_line += scr_width)
			for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
				*dst = ((*dst >> 1) & 0x7bef) + color;

	/* mix colors for any other alpha values */
	} else {
		int max_minus_alpha = 255 - alpha;
		color = blend(color, alpha);
		for (y = y1; y <= y2; y++, dst_line += scr_width)
			for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
				*dst = blend(*dst, max_minus_alpha) + color;
	}

#endif

	return 0;
}


static s32 scr_draw_slice_16(struct gfx_ds_data *s, s16 x, s16 y, s16 w, s16 h,
                             s16 sx, s16 sy, s16 sw, s16 sh,
                             struct gfx_ds *img, u8 alpha) {

	s32 type  = img->handler->get_type(img->data);
	s32 img_w = img->handler->get_width(img->data);
	s32 img_h = img->handler->get_height(img->data);

	switch (type) {
	case GFX_IMG_TYPE_RGB16:
		{
			u16 *src = img->handler->map(img->data);
			paint_scaled_img_rgb16(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
			break;
		}

	case GFX_IMG_TYPE_RGBA32:
		{
			u32 *src = img->handler->map(img->data);
			paint_scaled_img_rgba32(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
			break;
		}

	case GFX_IMG_TYPE_YUV420:
		{
			u16 *src = cache->get_elem(imgcache, img->cache_idx, 123);
			if (!src) {
				src = malloc(img_w*img_h*2);
				img->cache_idx = cache->add_elem(imgcache, src, img_w*img_h*2, 123, NULL);
			}
			convert_yuv420_to_16(img_w, img_h, img->handler->map(img->data), src);
			paint_scaled_img_rgb16(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
			break;
		}
	}

	return 0;
}

static s32 scr_draw_img_16(struct gfx_ds_data *s, s16 x, s16 y, s16 w, s16 h,
                             struct gfx_ds *img, u8 alpha) {
	return scr_draw_slice_16(s, x, y, w, h, 0, 0,
	                         img->handler->get_width(img->data),
	                         img->handler->get_height(img->data),
	                         img, alpha);
}

static s32 scr_draw_string_16(struct gfx_ds_data *ds, s16 x, s16 y,
                             s32 fg_rgba, u32 bg_rgba, s32 fnt_id,
                             char *str_signed) {
	struct font *font = fontman->get_by_id(fnt_id);
	s32 *wtab = font->width_table;
	s32 *otab = font->offset_table;
	s32 img_w = font->img_w;
	s32 img_h = font->img_h;
	u16 *dst = scr_adr + y*scr_width + x;
	u8  *str = (u8 *)str_signed;
	u8  *src = font->image;
	u8  *s;
	u16 *d;
	s16 i, j;
	s32 w;
	s32 h = font->img_h;
	u16 color = RGBA_TO_RGB16(fg_rgba);
	if (!str) return -1;

	/* check top clipping */
	if (y < clip_y1) {
		src += (clip_y1 - y)*img_w;   /* skip upper lines in font image */
		h   -= (clip_y1 - y);         /* decrement number of lines to draw */
		dst += (clip_y1 - y)*scr_width;
	}

	/* check bottom clipping */
	if (y+img_h - 1 > clip_y2)
		h -= (y + img_h - 1 - clip_y2);  /* decr. number of lines to draw */

	if (h < 1) return -1;

	/* skip characters that are completely hidden by the left clipping border */
	while (*str && (x+wtab[(int)(*str)] < clip_x1)) {
		x+=wtab[(int)(*str)];
		dst+=wtab[(int)(*str)];
		str++;
	}

	/* draw left cutted character */
	if (*str && (x+wtab[(int)(*str)] - 1 <= clip_x2) && (x < clip_x1)) {
		w = wtab[(int)(*str)] - (clip_x1 - x);
		s = src + otab[(int)(*str)] + (clip_x1 - x);
		d = dst + (clip_x1 - x);
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				if (s[i]) d[i] = color;
			}
			s = s + img_w;
			d = d + scr_width;
		}
		dst += wtab[(int)(*str)];
		x   += wtab[(int)(*str)];
		str++;
	}

	/* draw horizontally full visible characters */
	while (*str && (x + wtab[(int)(*str)] - 1 < clip_x2)) {
		w = wtab[(int)(*str)];
		s = src + otab[(int)(*str)];
		d = dst;
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				if (s[i]) d[i] = color;
			}
			s = s + img_w;
			d = d + scr_width;
		}
		dst += wtab[(int)(*str)];
		x   += wtab[(int)(*str)];
		str++;
	}

	/* draw right cutted character */
	if (*str) {
		w = wtab[(int)(*str)];
		s = src + otab[(int)(*str)];
		d = dst;
		if (x + w - 1 > clip_x2) {
			w -= x + w - 1 - clip_x2;
		}
		if (x < clip_x1) {    /* check if character is also left-cutted */
			w -= clip_x1 - x;
			s += clip_x1 - x;
			d += clip_x1 - x;
		}
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				if (s[i]) d[i] = color;
			}
			s += img_w;
			d += scr_width;
		}
	}
	return 0;
}


static void scr_push_clipping(struct gfx_ds_data *s, s32 x, s32 y, s32 w, s32 h) {
	clip->push(x, y, x + w - 1, y + h - 1);
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static void scr_pop_clipping(struct gfx_ds_data *s) {
	clip->pop();
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static void scr_reset_clipping(struct gfx_ds_data *s) {
	clip->reset();
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static s32 scr_get_clip_x(struct gfx_ds_data *s) {
	return clip_x1;
}


static s32 scr_get_clip_y(struct gfx_ds_data *s) {
	return clip_y1;
}


static s32 scr_get_clip_w(struct gfx_ds_data *s) {
	return clip_x2 - clip_x1 + 1;
}


static s32 scr_get_clip_h(struct gfx_ds_data *s) {
	return clip_y2 - clip_y1 + 1;
}


static void scr_set_mouse_pos(struct gfx_ds_data *s, s32 x, s32 y) {
	scrdrv->set_mouse_pos(x, y);
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static struct gfx_ds_data *create(s32 width, s32 height, struct gfx_ds_handler **handler) {
	scrdrv->set_screen(width, height, 16);
	scr_adr    = scrdrv->get_buf_adr();
	scr_width  = scrdrv->get_scr_width();
	scr_height = scrdrv->get_scr_height();

	if (scrdrv->get_scr_depth() == 16) scr_type = GFX_IMG_TYPE_RGB16;
	else return NULL;

	clip->set_range(0, 0, scr_width - 1, scr_height - 1);
	return (void *)1;
}

static s32 register_gfx_handler(struct gfx_ds_handler *handler) {
	handler->get_width      = scr_get_width;
	handler->get_height     = scr_get_height;
	handler->get_type       = scr_get_type;
	handler->destroy        = scr_destroy;
	handler->map            = scr_map;
	handler->update         = scr_update;
	handler->draw_hline     = scr_draw_hline_16;
	handler->draw_vline     = scr_draw_vline_16;
	handler->draw_fill      = scr_draw_fill_16;
	handler->draw_slice     = scr_draw_slice_16;
	handler->draw_img       = scr_draw_img_16;
	handler->draw_string    = scr_draw_string_16;
	handler->push_clipping  = scr_push_clipping;
	handler->pop_clipping   = scr_pop_clipping;
	handler->reset_clipping = scr_reset_clipping;
	handler->get_clip_x     = scr_get_clip_x;
	handler->get_clip_y     = scr_get_clip_y;
	handler->get_clip_w     = scr_get_clip_w;
	handler->get_clip_h     = scr_get_clip_h;
	handler->set_mouse_pos  = scr_set_mouse_pos;
	return 0;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct gfx_handler_services services = {
	create,
	register_gfx_handler,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_gfxscr16(struct dope_services *d) {

	scrdrv  = d->get_module("ScreenDriver 1.0");
	fontman = d->get_module("FontManager 1.0");
	clip    = d->get_module("Clipping 1.0");
	cache   = d->get_module("Cache 1.0");

	imgcache = cache->create(100, 1000*1000);

	init_yuv2rgb();

	d->register_module("GfxScreen16 1.0", &services);
	return 1;
}
