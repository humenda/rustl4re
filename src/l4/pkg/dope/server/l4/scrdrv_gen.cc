/*
 * \brief   DOpE screen driver module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This component  is the screen  output interface of
 * DOpE. It handles two frame buffers: the physically
 * visible frame buffer and a  background buffer. All
 * graphics  operations are applied to the background
 * buffer and transfered to the physical frame buffer
 * afterwards. This component is also responsible for
 * drawing the mouse cursor.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <l4/sys/kdebug.h>
#include <l4/sys/cache.h>
#include <l4/re/c/util/video/goos_fb.h>
#include <l4/util/util.h>

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "dopestd.h"
#include "scrdrv.h"
#include "clipping.h"
#include "gfx_colors.h"

#if USE_RT_MON
#include <l4/rt_mon/histogram2d.h>


extern rt_mon_histogram2d_t * hist2dxy;
#endif

/* XXX in startup.c (we need a property mgr?) */
extern int config_use_vidfix;
extern int config_nocursor;

/*** VARIABLES OF THE SCREEN MODULE ***/

static s32    scr_width, scr_height;         /* screen dimensions              */
static s32    scr_depth;                     /* color depth                    */
static s32    scr_linelength;                /* bytes per scanline             */
static s32    scr_bytes_per_scanline;        /* bytes per scanline             */
static void  *scr_adr;                       /* physical screen adress         */
static void  *buf_adr;                       /* adress of double screen buffer */
static s32    curr_mx = 100, curr_my = 100;  /* current mouse cursor position  */

static struct clipping_services *clip;

/*** PUBLIC ***/

extern "C" int init_scrdrv(struct dope_services *d);

static u32 bg_buffer[16][16];

extern short smallmouse_trp;
extern short bigmouse_trp;

/*** Abstract drawing interface for the scree driver */
class Scr_drv
{
public:
	virtual void draw_cursor(short *data, s32 x, s32 y) const = 0;
	virtual void save_background(s32 x, s32 y) const = 0;
	virtual void restore_background(s32 x, s32 y) const = 0;
	virtual ~Scr_drv() {}
};


/*** Color space dependent drawing implementation */
template< typename _Traits >
class Scr_drv_x : public Scr_drv
{
public:
	typedef _Traits Traits;
	typedef typename Traits::Pixel Pixel;
	typedef typename Traits::Color Color;

	void draw_cursor(short *data, s32 x, s32 y) const {
		short i, j;
		Pixel *dst, *d;
		dst = (Pixel*)buf_adr + y*scr_width + x;
		short const *s = data;
		short w = *(data++), h = *(data++);
		short linelen = w;

		if (x >= scr_width)  return;
		if (y >= scr_height) return;
		if (x >= scr_width - 16)  linelen = scr_width - x;
		if (y >= scr_height - 16) h = scr_height - y;

		for (j = 0; j < h; j++) {
			d = dst; s = data;
			for (i = 0; i < linelen; i++) {
				if (*s) *d = Conv<Rgb16, Traits>::convert(*s);
				d++; s++;
			}
			dst  += scr_width;
			data += w;
		}
	}

	void save_background(s32 x, s32 y) const {
		Pixel const *src = (Pixel const *)buf_adr + y*scr_width + x;
		Color *dst = (Color*)&bg_buffer;
		Pixel const *s;
		short h = 16;
		int i, j;

		if (y >= scr_height - 16) h = scr_height - y;

		for (j = 0; j < h; j++) {
			s = src;
			for (i = 0; i < 16; i++) {
				*(dst++) = *(s++);
			}
			src += scr_width;
		}
	}


	void restore_background(s32 x, s32 y) const {
		Color const *src = (Color*)&bg_buffer;
		Pixel *dst = (Pixel*)buf_adr + y*scr_width + x;
		Pixel *d;
		short h = 16;
		int i, j;
		
		if (y >= scr_height - 16) h = scr_height - y;

		for (j = 0; j < h; j++) {
			d = dst;
			for (i = 0; i < 16; i++) {
				*(d++) = *(src++);
			}
			dst += scr_width;
		}
	}
};


/*** points to the current incarnation of the drawing iterface */
static Scr_drv *scrdrv;

/*** MAKE CHANGES ON THE SCREEN VISIBLE (BUFFERED OUTPUT) ***/
static void update_area(long x1, long y1, long x2, long y2) {
	long dx;
	long dy;
	long v;
	long i, j;
	int const bpp = (scr_depth+7) / 8;
	u8 *src, *dst;
	u32 *s, *d;
	int cursor_visible = 0;

	if (!config_nocursor
	    && (curr_mx < x2) && (curr_mx + 16 > x1)
	    && (curr_my < y2) && (curr_my + 16 > y1)) {
		scrdrv->save_background(curr_mx, curr_my);
		scrdrv->draw_cursor(&bigmouse_trp, curr_mx, curr_my);
		cursor_visible = 1;
	}

	/* apply clipping to specified area */
	if (x1 < (v = clip->get_x1())) x1 = v;
	if (y1 < (v = clip->get_y1())) y1 = v;
	if (x2 > (v = clip->get_x2())) x2 = v;
	if (y2 > (v = clip->get_y2())) y2 = v;

	dx = x2 - x1;
	dy = y2 - y1;
	if (dx < 0) return;
	if (dy < 0) return;

	/* determine offset of left top corner of the area to update */
	src = (u8*)buf_adr + (y1*scr_width + x1) * bpp;
	dst = (u8*)scr_adr + (y1*scr_linelength + x1) * bpp;

	dx  = (dx + 1) * bpp + ((adr)dst & (sizeof(u32)-1));
	dx  = (dx + sizeof(u32) - 1) / sizeof(u32);
	src = (u8 *)((adr)src & ~(sizeof(u32)-1));
	dst = (u8 *)((adr)dst & ~(sizeof(u32)-1));

#if USE_RT_MON
	rt_mon_hist2d_start(hist2dxy);
#endif
	for (j = dy + 1; j--; ) {

		/* copy line */
		d = (u32 *)dst; s = (u32 *)src;
		for (i = dx; i--; ) *(d++) = *(s++);
//      memcpy(dst, src, dx*2);

		src += scr_width * bpp;
		dst += scr_linelength * bpp;
	}
#if 0
	l4_sys_cache_clean_range((unsigned long)((u8*)scr_adr + (y1*scr_linelength + x1) * bpp),
	                         (unsigned long)dst - (scr_linelength * bpp));
#endif

#if USE_RT_MON
	{
		long long x[2];
		signed long long now, diff;

		/* get current timestamp */
		now = rt_mon_hist2d_measure(hist2dxy);
		x[0] = dx * 2 + 2;
		x[1] = dy + 1;

		/* only collect data from the first 100 points */
		if (rt_mon_hist2d_get_data(hist2dxy, x, 1) < 100) {

			/* '1000' converts from µs to ns when process times are used */
			diff = now - hist2dxy->start - hist2dxy->time_ovh;
			rt_mon_hist2d_insert_data(hist2dxy, x, 0,
			                          (diff * 1000) / (x[0] * x[1])
			);

			/* update counter too */
			rt_mon_hist2d_insert_data(hist2dxy, x, 1, 1);
		}
	}
#endif

	if (cursor_visible) {
		scrdrv->restore_background(curr_mx, curr_my);
	}
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

#if 0
#ifndef ARCH_arm
static int
vc_map_video_mem(l4_addr_t paddr, l4_size_t size,
                 l4_addr_t *vaddr, l4_addr_t *offset)
{
  (void)paddr; (void)size; (void)vaddr; (void)offset;
  printf("%s %d\n", __func__, __LINE__); 

	printf("dope: paddr=%lx size=%ldKiB\n", paddr, (unsigned long)size >> 10);
	if ((*vaddr = l4io_request_mem_region(paddr, size, L4IO_MEM_WRITE_COMBINED)) == 0)
		Panic("Can't request memory region from l4io.");
	*offset = 0;

	printf("Mapped video memory at %08lx to %08lx+%06lx [%ldkB] via L4IO\n",
	       paddr, *vaddr, *offset, (unsigned long)size >> 10);

	printf("mapping: vaddr=0x%lx size=%ld(0x%lx) offset=%ld(0x%lx)\n",
	       *vaddr, (unsigned long)size, (unsigned long)size, *offset, *offset);

	return 0;
}
#endif
#endif


/*** wide spread video modes ***/
static Scr_drv_x<Rgb15> drv_rgb15;
static Scr_drv_x<Bgr15> drv_bgr15;
static Scr_drv_x<Rgb16> drv_rgb16;
static Scr_drv_x<Rgb24> drv_rgb24;
static Scr_drv_x<Rgb32> drv_rgb32;

l4re_util_video_goos_fb_t goosfb;
int goosfb_initialized;
void *fb_buf;

static void *__refresher(void *)
{
  while (1)
    {
      l4_sleep(50);
      l4re_util_video_goos_fb_refresh(&goosfb, 0, 0, scr_width, scr_height);
    }
  return NULL;
}

/*** SET UP SCREEN ***/
static long set_screen(long width, long height, long depth)
{
  (void)width;
  (void)height;
  (void)depth;

  l4re_video_view_info_t fbinfo;

  if (!goosfb_initialized
      && l4re_util_video_goos_fb_setup_name(&goosfb, "fb"))
    {
      printf("video setup failed\n");
      return 0;
    }
  goosfb_initialized = 1;

  fb_buf = l4re_util_video_goos_fb_attach_buffer(&goosfb);
  if (!fb_buf)
    {
      printf("video memory init failed\n");
      return 0;
    }

  if (l4re_util_video_goos_fb_view_info(&goosfb, &fbinfo))
    {
      printf("l4re_fb_info failed\n");
      return 1;
    }

  if (l4re_util_video_goos_fb_refresh(&goosfb, 0, 0, 1, 1) == -L4_EOK)
    {
      // refreshing works, so it looks like we have do actually also do it
      printf("Attention: I'm using a framebuffer refresh which drains CPU power!\n");
      pthread_t t;
      pthread_create(&t, NULL, __refresher, NULL);
    }


#if 0

#ifndef ARCH_arm
	l4util_mb_vbe_ctrl_t *vbe;
	l4util_mb_vbe_mode_t *vbi;
	l4util_mb_info_t *mbi = l4env_multiboot_info;

	l4_addr_t gr_vbase;
	l4_addr_t gr_voffs;

	if (!(mbi->flags & L4UTIL_MB_VIDEO_INFO) || !(mbi->vbe_mode_info)) {
		printf("Did not find VBE info block in multiboot info. "
		       "GRUB has to set the \n"
		       "video mode with the vbeset command.\n");
		enter_kdebug("PANIC");
		return 0;
	}

	vbe = (l4util_mb_vbe_ctrl_t*) (unsigned long)mbi->vbe_ctrl_info;
	vbi = (l4util_mb_vbe_mode_t*) (unsigned long)mbi->vbe_mode_info;

	vc_map_video_mem((vbi->phys_base >> L4_SUPERPAGESHIFT) << L4_SUPERPAGESHIFT,
                         64*1024*vbe->total_memory,
	                 &gr_vbase, &gr_voffs);

        gr_voffs += vbi->phys_base & ((1<< L4_SUPERPAGESHIFT)-1);

	if (!config_use_vidfix) gr_voffs = 0;

	printf("Frame buffer base:  %p\n"
	       "Resolution:         %dx%dx%d\n"
	       "Bytes per scanline: %d\n", (void *)(gr_vbase + gr_voffs),
	       vbi->x_resolution, vbi->y_resolution,
	       vbi->bits_per_pixel, vbi->bytes_per_scanline );

	scr_adr         = (void *)(gr_vbase + gr_voffs);
	scr_height      = vbi->y_resolution;
	scr_width       = vbi->x_resolution;
	scr_depth       = vbi->bits_per_pixel;
	scr_bytes_per_scanline = vbi->bytes_per_scanline;
	scr_linelength  = scr_bytes_per_scanline / (scr_depth/8); //scr_width;

	if (config_use_vidfix) {
	    scr_linelength = scr_bytes_per_scanline/2;
	}
#endif
#endif

        scr_adr         = fb_buf;
        scr_height      = fbinfo.height;
        scr_width       = fbinfo.width;
        scr_depth       = l4re_video_bits_per_pixel(&fbinfo.pixel_info);
        scr_bytes_per_scanline = fbinfo.bytes_per_line;
        scr_linelength  = scr_bytes_per_scanline / ((scr_depth + 7) >> 3); //scr_width;

        printf("Screen-Info: %ldx%ld@%d\n",
               fbinfo.width, fbinfo.height, scr_depth);

	/* get memory for the screen buffer */
	buf_adr = malloc((scr_height+16)*scr_bytes_per_scanline);
	if (!buf_adr) {
		INFO(printf("Screen(set_screen): out of memory!\n");)
		return 0;
	}

	/* select the drawing routines for the current color mode */
	switch (scr_depth) {
		case 15: scrdrv = &drv_rgb15; break;
		case 16: scrdrv = &drv_rgb16; break;
		case 24: scrdrv = &drv_rgb24; break;
		case 32: scrdrv = &drv_rgb32; break;
		default: enter_kdebug("unsuported color depth"); return 0;
	}

	printf("Current video mode is %dx%d@%d\n"
	       "  Bytes per scanline: %d\n"
	       "  Scanline length:    %d\n",
	       scr_width, scr_height, scr_depth,
	       scr_bytes_per_scanline, scr_linelength);

	return 1;
}


/*** DEINITIALISATION ***/
static void restore_screen(void) {
	/* nothing to restore - sdl is too good */
	if (buf_adr) free(buf_adr);
	buf_adr = NULL;
}


/*** PROVIDE INFORMATION ABOUT THE SCREEN ***/
static long  get_scr_width  (void) {return scr_width;}
static long  get_scr_height (void) {return scr_height;}
static long  get_scr_depth  (void) {return scr_depth;}
static void *get_scr_adr    (void) {return scr_adr;}
static void *get_buf_adr    (void) {return buf_adr;}




/*** SET MOUSE CURSOR TO THE SPECIFIED POSITION ***/
static void set_mouse_pos(long mx, long my) {
	int old_mx = curr_mx, old_my = curr_my;

	/* do not redraw mouse cursor if it did not move */
	if ((curr_mx == mx) && (curr_my == my)) return;

	curr_mx = mx;
	curr_my = my;
	update_area(old_mx,  old_my,  old_mx  + 15, old_my  + 15);
	update_area(curr_mx, curr_my, curr_mx + 15, curr_my + 15);
}


/*** SET NEW MOUSE SHAPE ***/
static void set_mouse_shape(void *new_shape) {
  (void)new_shape;
	/* not built in yet... */
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scrdrv_services services = {
	set_screen:         set_screen,
	restore_screen:     restore_screen,
	get_scr_width:      get_scr_width,
	get_scr_height:     get_scr_height,
	get_scr_depth:      get_scr_depth,
	get_scr_adr:        get_scr_adr,
	get_buf_adr:        get_buf_adr,
	update_area:        update_area,
	set_mouse_pos:      set_mouse_pos,
	set_mouse_shape:    set_mouse_shape,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_scrdrv(struct dope_services *d) {

	clip = (clipping_services*)d->get_module("Clipping 1.0");

	d->register_module("ScreenDriver 1.0", &services);
	return 1;
}
