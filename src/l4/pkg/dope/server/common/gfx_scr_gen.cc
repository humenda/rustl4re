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

#include <cstdlib>
#include <cstdio>

#include "dopestd.h"
#include "scrdrv.h"
#include "cache.h"
#include "fontman.h"
#include "clipping.h"
#include "gfx.h"
#include "gfx_handler.h"
#include "gfx_colors.h"

/*** Global information for DOpE modules */

static struct scrdrv_services   *scrdrv;
static struct fontman_services  *fontman;
static struct clipping_services *clip;
static struct cache_services    *cache;

static CACHE *imgcache;

extern "C" int init_gfxscr16(struct dope_services *d);

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
	r = (u16 *) table_rV[V];\
	g = (u16 *) (((u8 *)table_gU[U]) + table_gV[V]);\
	b = (u16 *) table_bU[U];

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

static void convert_yuv420_to_16(int width, int height, u8 const *src_yuv420, u16 *dst_rgb16) {
	void *dst =dst_rgb16;
	u8 const *py = src_yuv420;
	u8 const *pu = src_yuv420 + width*height;
	u8 const *pv = src_yuv420 + width*height + width*height/4;
	int rgb_stride = width*2;
	int y_stride = width;
	int uv_stride = width/2;
	int U, V, Y;
	u16 * r, * g, * b;
	u16 * dst_1, * dst_2;

	height >>= 1;
	do {

		u8 const * py_1 = py;
		u8 const * py_2 = py + y_stride;
		u8 const * pu_  = pu;
		u8 const * pv_  = pv;
		void * _dst_1 = dst;
		void * _dst_2 = ((u8 *)dst) + rgb_stride;
		int width_ = width;

		width_ >>= 3;
		dst_1 = (u16*)_dst_1;
		dst_2 = (u16*)_dst_2;

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


/*** Global data for the gfx_scr singleton */
static void *scr_adr;
static s32  clip_x1, clip_y1, clip_x2, clip_y2;
static s32  scr_width, scr_height, scr_type;
static s32 scale_xbuf[2000];


/*** PUBLIC gfx_ds_handler functions (colormode independent) */

static void *scr_map(struct gfx_ds_data *) {
	return scr_adr;
}


static s32 scr_get_width(struct gfx_ds_data *) {
	return scr_width;
}


static s32 scr_get_height(struct gfx_ds_data *) {
	return scr_height;
}


static s32 scr_get_type(struct gfx_ds_data *) {
	return scr_type;
}

static void scr_destroy(struct gfx_ds_data *) {
	scrdrv->restore_screen();
}

static void scr_update(struct gfx_ds_data *, s32 x, s32 y, s32 w, s32 h) {
	scrdrv->update_area(x, y, x + w - 1, y + h - 1);
}


static void scr_push_clipping(struct gfx_ds_data *, s32 x, s32 y, s32 w, s32 h) {
	clip->push(x, y, x + w - 1, y + h - 1);
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static void scr_pop_clipping(struct gfx_ds_data *) {
	clip->pop();
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static void scr_reset_clipping(struct gfx_ds_data *) {
	clip->reset();
	clip_x1 = clip->get_x1();
	clip_y1 = clip->get_y1();
	clip_x2 = clip->get_x2();
	clip_y2 = clip->get_y2();
}


static s32 scr_get_clip_x(struct gfx_ds_data *) {
	return clip_x1;
}


static s32 scr_get_clip_y(struct gfx_ds_data *) {
	return clip_y1;
}


static s32 scr_get_clip_w(struct gfx_ds_data *) {
	return clip_x2 - clip_x1 + 1;
}


static s32 scr_get_clip_h(struct gfx_ds_data *) {
	return clip_y2 - clip_y1 + 1;
}


static void scr_set_mouse_pos(struct gfx_ds_data *, s32 x, s32 y) {
	scrdrv->set_mouse_pos(x, y);
}

/*** PRIVATE: CLIP IMAGE AGAINST CLIPPING AREA ***/
static int clip_img(int clip_x1, int clip_y1, int clip_x2, int clip_y2,
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


/*** Colordepth dependent drawing routines */
template< typename _Traits >
class Gfx_scr
{
private:
	typedef _Traits Traits;
	typedef typename Traits::Color Color;
	typedef typename Traits::Pixel Pixel;

private:
	/*** DRAW A SOLID HORIZONTAL LINE IN 16BIT COLOR MODE ***/
	static void solid_hline(Pixel *dst, u32 width, Color col) {
		for (;width--;dst++) *dst = col;
	}


	/*** DRAW A SOLID VERTICAL LINE IN 16BIT COLOR MODE ***/
	static void solid_vline(Pixel *dst, u32 height, u32 scr_w, Color col) {
		for (;height--;dst+=scr_w) *dst = col;
	}


	/*** DRAW A TRANSPARENT HORIZONTAL LINE IN 16BIT COLOR MODE ***/
	static void mixed_hline(Pixel *dst, u32 width, Color mixcol) {
		mixcol=(mixcol&Traits::C_space::Mix_mask)>>1;
		for (;width--;dst++) *dst = (((*dst)&Traits::C_space::Mix_mask)>>1) + mixcol;
	}


	/*** DRAW A TRANSPARENT VERTICAL LINE IN 16BIT COLOR MODE ***/
	static void mixed_vline(Pixel *dst, u32 height, u32 scr_w, Color mixcol) {
		mixcol=(mixcol&Traits::C_space::Mix_mask)>>1;
		for (;height--;dst+=scr_w) *dst = (((*dst)&Traits::C_space::Mix_mask)>>1) + mixcol;
	}


	/*** DRAW CLIPPED 16BIT IMAGE TO 16BIT SCREEN ***/
	template<typename T>
	static void paint_img(int x, int y, int img_w, int img_h,
	                      typename T::Pixel src) {
		int i, j;
		int w = img_w, h = img_h;
		Pixel *dst, *d;
		typename T::Pixel const *s;
		int sx = 0, sy = 0;

		if (!clip_img(clip_x1, clip_y1, clip_x2, clip_y2,
		              &x, &y, &w, &h, &sx, &sy, 1, 1)) return;

		/* calculate start address */
		src += img_w*sy + sx;
		dst  = Pixel(scr_adr) + y*scr_width + x;

		/* paint... */
		for (j = h; j--; ) {

			/* copy line from image to screen */
			for (i = w, s = src, d = dst; i--;)
				Conv<T, Traits>::blit(*(s++), d++);
			src += img_w;
			dst += scr_width;
		}
	}


	/*** DRAW SCALED AND CLIPPED 16BIT IMAGE TO 16BIT SCREEN ***/
	template< typename T >
	static void paint_scaled_img(int x, int y, int w, int h,
	                             int linewidth, int sw, int sh,
	                             typename T::Pixel const *src) {
		int mx, my;
		int i, j;
		int sx = 0, sy = 0;
		Pixel *dst, *d;
		typename T::Pixel const *s;

		/* sanity check */
		if (!src) return;

//		/* use shortcut for non-scaled images */
//		if ((w == iw) && (h == ih)) {
//			paint_img_16(x, y, img_w, img_h, src);
//			return;
//		}

		mx = w ? ((long)sw<<16) / w : 0;
		my = h ? ((long)sh<<16) / h : 0;

		if (!clip_img(clip_x1, clip_y1, clip_x2, clip_y2,
		              &x, &y, &w, &h, &sx, &sy, mx, my)) return;

		/* calculate start address */
		dst = (Pixel*)scr_adr + y*scr_width + x;

		/* calculate x offsets */
		for (i = w; i--; sx += mx)
			scale_xbuf[i] = sx >> 16;

		/* draw scaled image */
		for (j = h; j--; sy += my, dst += scr_width) {
			s = src + ((sy>>16)*linewidth);
			d = dst;
			for (i = w; i--; )
				Conv<T, Traits>::blit(*(s + scale_xbuf[i]), d++);
		}
	}


public:
	/*****************************
	 *** GFX HANDLER FUNCTIONS ***
	 *****************************/

	static s32 scr_draw_hline(struct gfx_ds_data *s, s16 x, s16 y, s16 w, u32 rgba) {
	  (void)s;
		int beg_x, end_x;

		if (clip_y1 > y || clip_y2 < y) return 0;
		beg_x = MAX(x, clip_x1);
		end_x = MIN(x + w - 1, clip_x2);

		if (beg_x > end_x) return 0;
	
		if (Rgba32::A::get(rgba) > 127) {
			solid_hline((Pixel*)scr_adr + y*scr_width + beg_x, end_x - beg_x + 1, Conv<Rgba32, Traits>::convert(rgba));
		} else {
			mixed_hline((Pixel*)scr_adr + y*scr_width + beg_x, end_x - beg_x + 1, Conv<Rgba32, Traits>::convert(rgba));
		}
		return 0;
	}


	static s32 scr_draw_vline(struct gfx_ds_data *s, s16 x, s16 y, s16 h, u32 rgba) {
	  (void)s;
		s32 beg_y, end_y;

		if (clip_x1 > x || clip_x2 < x) return 0;
		beg_y = MAX(y, clip_y1);
		end_y = MIN(y + h - 1, clip_y2);
		if (beg_y > end_y) return 0;

		if (GFX_A(rgba) > 127) {
			solid_vline((Pixel*)scr_adr + beg_y*scr_width + x, end_y - beg_y + 1, scr_width, Conv<Rgba32, Traits>::convert(rgba));
		} else {
			mixed_vline((Pixel*)scr_adr + beg_y*scr_width + x, end_y - beg_y + 1, scr_width, Conv<Rgba32, Traits>::convert(rgba));
		}
		return 0;
	}


	static s32 scr_draw_fill(struct gfx_ds_data *s, s16 x1, s16 y1, s16 w, s16 h, u32 rgba) {
	  (void)s;
		s16 x, y;
		Pixel *dst, *dst_line;
		Color color;
		u16 alpha;
		s16 x2 = x1 + w - 1;
		s16 y2 = y1 + h - 1;

		/* check clipping */
		if (x1 < clip_x1) x1 = clip_x1;
		if (y1 < clip_y1) y1 = clip_y1;
		if (x2 > clip_x2) x2 = clip_x2;
		if (y2 > clip_y2) y2 = clip_y2;
		if (x1 > x2) return -1;
		if (y1 > y2) return -1;

		color = Conv<Rgba32, Traits>::convert(rgba);
		alpha = Rgba32::A::get(rgba);


		dst_line = (Pixel*)scr_adr + scr_width*y1;

		/* solid fill for 100% alpha */
		if (alpha == 0xff) {
			for (y = y1; y <= y2; y++, dst_line += scr_width)
				for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
					*dst = color;

		/* mix colors for 50% alpha */
		} else if (alpha == 0x7f) {
			color = (color&Traits::C_space::Mix_mask) >> 1; /* 50% alpha mode */
			for (y = y1; y <= y2; y++, dst_line += scr_width)
				for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
					*dst = ((*dst & Traits::C_space::Mix_mask) >> 1) + color;

		/* mix colors for any other alpha values */
		} else {
			int max_minus_alpha = 255 - alpha;
			color = Traits::blend(color, alpha);
			for (y = y1; y <= y2; y++, dst_line += scr_width)
				for (dst = dst_line + x1, x = x1; x <= x2; x++, dst++)
					*dst = Traits::blend(*dst, max_minus_alpha) + color;
		}

		return 0;
	}

	static s32 scr_draw_slice(struct gfx_ds_data *s, s16 x, s16 y, s16 w, s16 h,
	                             s16 sx, s16 sy, s16 sw, s16 sh,
	                             struct gfx_ds *img, u8 alpha) {
	  (void)s;
	  (void)alpha;

		s32 type  = img->handler->get_type(img->data);
		s32 img_w = img->handler->get_width(img->data);
		s32 img_h = img->handler->get_height(img->data);

		switch (type) {
		case GFX_IMG_TYPE_NATIVE:
			{
				typename Traits::Pixel const *src = (typename Traits::Pixel const *)(img->handler->map(img->data));
				paint_scaled_img<Traits>(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
				break;
			}

		case GFX_IMG_TYPE_RGB16:
			{
				Rgb16::Pixel const *src = (Rgb16::Pixel const *)img->handler->map(img->data);
				paint_scaled_img<Rgb16>(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
				break;
			}

		case GFX_IMG_TYPE_RGBA32:
			{
				Rgba32::Pixel const *src = (Rgba32::Pixel const *)img->handler->map(img->data);
				paint_scaled_img<Rgba32>(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
				break;
			}

		case GFX_IMG_TYPE_YUV420:
			{
			  Rgb16::Pixel *src = (Rgb16::Pixel *)cache->get_elem(imgcache, img->cache_idx, 123);
				if (!src) {
					src = (Rgb16::Pixel *)malloc(img_w*img_h*2);
					img->cache_idx = cache->add_elem(imgcache, src, img_w*img_h*2, 123, NULL);
				}
				convert_yuv420_to_16(img_w, img_h, (u8*)(img->handler->map(img->data)), src);
				paint_scaled_img<Rgb16>(x, y, w, h, img_w, sw, sh, src + img_w*sy + sx);
				break;
			}
		}

		return 0;
	}

	static s32 scr_draw_img(struct gfx_ds_data *s, s16 x, s16 y, s16 w, s16 h,
	                             struct gfx_ds *img, u8 alpha) {
		return scr_draw_slice(s, x, y, w, h, 0, 0,
		                      img->handler->get_width(img->data),
		                      img->handler->get_height(img->data),
		                      img, alpha);
	}

	static s32 scr_draw_string(struct gfx_ds_data *ds, s16 x, s16 y, s32 fg_rgba, u32 bg_rgba, s32 fnt_id, char *str) {
	  (void)ds; (void)bg_rgba;
		struct font *font = fontman->get_by_id(fnt_id);
		s32 *wtab = font->width_table;
		s32 *otab = font->offset_table;
		s32 img_w = font->img_w;
		s32 img_h = font->img_h;
		Pixel *dst = (Pixel*)scr_adr + y*scr_width + x;
		u8  const *src = font->image;
		u8  const *s;
		Pixel *d;
		s16 i, j;
		s32 w;
		s32 h = font->img_h;
		Color color = Conv<Rgba32, Traits>::convert(fg_rgba);
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

public:
	static gfx_ds_handler handler;
};


template< typename T >
gfx_ds_handler Gfx_scr<T>::handler = {
	scr_get_width,
	scr_get_height,
	scr_get_type,
	scr_destroy,
	scr_map,
	0,
	scr_update,
	0,
	0,
	Gfx_scr<T>::scr_draw_hline,
	Gfx_scr<T>::scr_draw_vline,
	Gfx_scr<T>::scr_draw_fill,
	Gfx_scr<T>::scr_draw_slice,
	Gfx_scr<T>::scr_draw_img,
	Gfx_scr<T>::scr_draw_string,
	scr_push_clipping,
	scr_pop_clipping,
	scr_reset_clipping,
	scr_get_clip_x,
	scr_get_clip_y,
	scr_get_clip_w,
	scr_get_clip_h,
	0,
	scr_set_mouse_pos
};


/*** defintion for wide spread video modes */
typedef Gfx_scr<Rgb15> Gfxs15;
typedef Gfx_scr<Rgb16> Gfxs16;
typedef Gfx_scr<Rgb32> Gfxs32;
typedef Gfx_scr<Rgb24> Gfxs24;


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static struct gfx_ds_data *create(s32 width, s32 height, gfx_ds_handler **handler) {
	scrdrv->set_screen(width, height, 16);
	scr_adr    = scrdrv->get_buf_adr();
	scr_width  = scrdrv->get_scr_width();
	scr_height = scrdrv->get_scr_height();

	if (scrdrv->get_scr_depth() == 16) scr_type = GFX_IMG_TYPE_RGB16;
	else scr_type = GFX_IMG_TYPE_NATIVE;

	clip->set_range(0, 0, scr_width - 1, scr_height - 1);
	switch (scrdrv->get_scr_depth()) {
		case 15: *handler = &Gfxs15::handler; break;
		case 16: *handler = &Gfxs16::handler; break;
		case 24: *handler = &Gfxs24::handler; break;
		case 32: *handler = &Gfxs32::handler; break;
		default:
			printf("ERROR: unsupported videon mode %ld\n", scrdrv->get_scr_depth()); 
			return 0;
	}
	return (gfx_ds_data *)1;
}

static s32 register_gfx_handler(struct gfx_ds_handler *handler) {
  (void)handler;
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

	scrdrv  = (scrdrv_services*)d->get_module("ScreenDriver 1.0");
	fontman = (fontman_services*)d->get_module("FontManager 1.0");
	clip    = (clipping_services*)d->get_module("Clipping 1.0");
	cache   = (cache_services*)d->get_module("Cache 1.0");

	imgcache = cache->create(100, 1000*1000);

	init_yuv2rgb();

	d->register_module("GfxScreen16 1.0", &services);
	return 1;
}
