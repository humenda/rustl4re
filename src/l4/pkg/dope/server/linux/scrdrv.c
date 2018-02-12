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
 * It uses SDL for accessing the screen under linux.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** GENERAL INCLUDES ***/
#include <signal.h>
#include <stdio.h>

/*** SDL INCLUDES ***/
#include <SDL/SDL.h>

/*** LOCAL INCLUDES ***/
#include "dopestd.h"
#include "scrdrv.h"
#include "clipping.h"

#ifndef SDL_DISABLE
#define SDL_DISABLE 0
#endif
#define SCR_DEBUG 0

static long scr_width,scr_height,scr_depth;
static void *scr_adr;
static long curr_mx = 100,curr_my = 100;
static SDL_Surface *screen;
static struct clipping_services *clip;

int init_scrdrv(struct dope_services *d);



static void draw_cursor(short *data,long x,long y) {
	static int i,j;
	short *dst = (short *)scr_adr + y*scr_width + x;
	short *d;
	short *s = data;
	int w = *(data++), h = *(data++);
	int linelen = w;
	if (x >= scr_width) return;
	if (y >= scr_height) return;
	if (x >= scr_width  - 16) linelen = scr_width - x;
	if (y >= scr_height - 16) h = scr_height - y;
	for (j = 0; j < h; j++) {
		d = dst; s = data;
		for (i = 0; i < linelen; i++) {
			if (*s) *d = *s;
			d++; s++;
		}
		dst  += scr_width;
		data += w;
	}
}


static short bg_buffer[20][16];

static void save_background(long x, long y) {
	short *src = (short *)scr_adr + y*scr_width + x;
	short *dst = (short *)&bg_buffer;
	short *s;
	static int i, j;
	int h = 16;
	if (y >= scr_height - 16) h = scr_height - y;
	for (j = 0; j < h; j++) {
		s = src;
		for (i = 0; i < 16; i++) {
			*(dst++) = *(s++);
		}
		src += scr_width;
	}
}


static void restore_background(long x, long y) {
	short *src = (short *)&bg_buffer;
	short *dst = (short *)scr_adr + y*scr_width + x;
	short *d;
	static int i, j;
	int h = 16;
	if (y >= scr_height - 16) h = scr_height - y;
	for (j = 0; j < h; j++) {
		d = dst;
		for (i = 0; i < 16; i++) {
			*(d++) = *(src++);
		}
		dst += scr_width;
	}
}


extern short smallmouse_trp;
extern short bigmouse_trp;


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** SET UP SCREEN ***/
static long set_screen(long width, long height, long depth) {
	scr_width  = width;
	scr_height = height;
	scr_depth  = depth;
	if (SCR_DEBUG) {
		screen = SDL_SetVideoMode(scr_width, scr_height*2 + 100, scr_depth, SDL_SWSURFACE);
	} else {
		screen = SDL_SetVideoMode(scr_width, scr_height + 20, scr_depth, SDL_SWSURFACE);
	}
	if (!screen) return 0;
	scr_adr = screen->pixels;
	SDL_ShowCursor(SDL_DISABLE);
	return 1;
}


/*** DEINITIALISATION ***/
static void restore_screen(void) {
	/* nothing to restore - sdl is too good */
}


/*** PROVIDE INFORMATION ABOUT THE SCREEN ***/
static long  get_scr_width  (void)  {return scr_width;}
static long  get_scr_height (void)  {return scr_height;}
static long  get_scr_depth  (void)  {return scr_depth;}
static void *get_scr_adr    (void)  {return scr_adr;}
static void *get_buf_adr    (void)  {return scr_adr;}


#if (SCR_DEBUG)
static void shade_area(long x1, long y1, long x2, long y2) {
	u16 *dst = scr_adr;
	u16 i;
	if (!SCR_DEBUG) return;
	if (y1 < 0) y1 = 0;
	if (y2 < 0) y2 = 0;
	if (y1 >= scr_height) y1 = scr_height - 1;
	if (y2 >= scr_height) y2 = scr_height - 1;
	dst += scr_width*scr_height;
	dst += y1*scr_width;
	for (; y1 < y2; y1++) {
		for (i = x1; i < x2; i++) dst[i] += 12;
		dst += scr_width;
	}
}
#endif


/*
static void scr_reset_shade(void) {
	u32  i = scr_width*scr_height;
	u16 *d = scr_adr;
	if (!SCR_DEBUG) return;
	d += scr_width*scr_height;
	while (i--) *(d++) = 0;
	SDL_UpdateRect(screen, 0, scr_height, scr_width, scr_height);
}
*/


/*** MAKE CHANGES ON THE SCREEN VISIBLE (BUFFERED OUTPUT) ***/
static void update_area(long x1, long y1, long x2, long y2) {
	long dx, dy, d;
	int  cursor_visible = 0;

	if ((curr_mx < x2) && (curr_mx + 16 > x1)
	 && (curr_my < y2) && (curr_my + 16 > y1)) {
		save_background(curr_mx, curr_my);
		draw_cursor(&bigmouse_trp, curr_mx, curr_my);
		cursor_visible = 1;
	}

	/* apply clipping to specified area */
	if (x1 < (d = clip->get_x1())) x1 = d;
	if (y1 < (d = clip->get_y1())) y1 = d;
	if (x2 > (d = clip->get_x2())) x2 = d;
	if (y2 > (d = clip->get_y2())) y2 = d;

	dx = x2 - x1;
	dy = y2 - y1;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;

	SDL_UpdateRect(screen, x1, y1, dx + 1, dy + 1);
#if (SCR_DEBUG)
	shade_area(x1, y1, x1 + dx + 1, y1 + dy + 1);
	SDL_UpdateRect(screen, x1, y1 + scr_height, dx + 1, dy + 1);
#endif
	if (cursor_visible) restore_background(curr_mx, curr_my);
}


/*** SET MOUSE CURSOR TO THE SPECIFIED POSITION ***/
static void set_mouse_pos(long mx, long my) {
	int old_mx = curr_mx, old_my = curr_my;

	/* check if position really changed */
	if ((curr_mx == mx) && (curr_my == my)) return;

	curr_mx = mx;
	curr_my = my;
	update_area(curr_mx, curr_my, curr_mx + 16, curr_my + 16);
	update_area(old_mx,  old_my,  old_mx  + 16, old_my  + 16);
}


/*** SET NEW MOUSE SHAPE ***/
static void set_mouse_shape(void *new_shape) {
	/* not built in yet... */
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scrdrv_services services = {
	set_screen,
	restore_screen,
	get_scr_width,
	get_scr_height,
	get_scr_depth,
	get_scr_adr,
	get_buf_adr,
	update_area,
	set_mouse_pos,
	set_mouse_shape,
};


/***********************
 *** SCREENSHOT HOOK ***
 ***********************/

/*** SIGNAL HANDLER FOR USR1: DUMP SCREENSHOT ***/
static void catch_usr1_signal(int signum) {
	char *scr_fname = "screen.pnm";
	FILE *fh;
	int i, j;
	u16 *s;   /* pixel source address */

	signal(SIGUSR1, catch_usr1_signal);

	fh = fopen(scr_fname, "w");
	printf("dumping screen to file %s\n", scr_fname);

	/* write header */
	fprintf(fh, "P3\n# CREATOR: DOpE\n%d %d\n255\n",
	            (int)scr_width, (int)scr_height);

	/* write pixel data */
	s = (u16 *)scr_adr;
	for (j = 0; j < scr_height; j++) for (i = 0; i < scr_width; i++, s++) {
		fprintf(fh, "%d\n", (*s & 0xf800) >> 8);
		fprintf(fh, "%d\n", (*s & 0x07e0) >> 3);
		fprintf(fh, "%d\n", (*s & 0x001f) << 3);
	}

	/* close file */
	fclose(fh);
}


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_scrdrv(struct dope_services *d) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) return 0;

	signal(SIGSEGV, SIG_DFL);

	/* install signal handler to take screenshots */
	signal(SIGUSR1, catch_usr1_signal);

	clip = d->get_module("Clipping 1.0");
	d->register_module("ScreenDriver 1.0", &services);
	return 1;
}
