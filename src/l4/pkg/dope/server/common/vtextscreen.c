/*
 * \brief   DOpE VTextScreen widget module
 * \date    2004-03-02
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

struct vtextscreen;
#define WIDGET struct vtextscreen

#include "dopestd.h"
#include "appman.h"
#include "widget_data.h"
#include "widget_help.h"
#include "gfx.h"
#include "redraw.h"
#include "vtextscreen.h"
#include "fontman.h"
#include "script.h"
#include "widman.h"
#include "thread.h"
#include "messenger.h"
#include "sharedmem.h"

static struct widman_services      *widman;
static struct script_services      *script;
static struct redraw_services      *redraw;
static struct appman_services      *appman;
static struct gfx_services         *gfx;
static struct fontman_services     *fontman;
static struct sharedmem_services   *shmem;
static struct thread_services      *thread;


struct vtextscreen_data {
	int        mode;                /* text mode                             */
	s32        xres, yres;          /* virtual screen dimensions             */
	SHAREDMEM *smb;                 /* shared representation buffer id       */
	char       smb_ident[64];       /* identifier for shared memory block    */
	u8        *buffer;              /* pointer to text representation buffer */
	char      *tmpstr;              /* string buffer for gfx output          */
	int        fn_w, fn_h;          /* size of monospaced character          */
	int        font_id;             /* font id of used monospaced font       */
	int        curs_x, curs_y;      /* cursor position                       */
	int        curs_nx, curs_ny;    /* new cursor position                   */
};

#define VTEXTSCR_MODE_C8A8PLN 1     /* 8bit characters, 8bit attributes, planar */

/*
 * Color table for terminal colors: base color brightness is 100, each
 * brightness step is 50. There are 3 brightness levels, maximum is 250.
 */

static u32 term_coltab[8] = {
	GFX_RGB(0,0,0),   GFX_RGB(155,0,0),   GFX_RGB(0,155,0),   GFX_RGB(155,155,0),
	GFX_RGB(0,0,155), GFX_RGB(155,0,155), GFX_RGB(0,155,155), GFX_RGB(155,155,155),
};

static u32 fg_coltab[3][8];  /* foreground colors for different brightness levels */
static u32 bg_coltab[6][8];  /* background colors for different brightness levels */

extern int config_transparency;

int init_vtextscreen(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** SET WIDGET SPECIFIC DATA TO DEFAULT ***/
static void set_default_values(VTEXTSCREEN *vts) {
	memset(vts->vd, 0, sizeof(struct vtextscreen_data));
	vts->vd->font_id = 1;
	vts->vd->fn_w = fontman->calc_str_width (vts->vd->font_id, "A");
	vts->vd->fn_h = fontman->calc_str_height(vts->vd->font_id, "A");
	vts->vd->curs_x = vts->vd->curs_y = -1;
}


/*** EXTRACT A SUBSTRING WITH CONSTANT ATTRIBUTES FROM TEXTBUFFER ***
 *
 * \param   cbuf   source character buffer
 * \param   abuf   source attribute buffer
 * \param   dst    destination string buffer
 * \param   max    maximum length of destination string
 * \return  length of extracted string
 */
static inline int extract_substring(u8 *cbuf, u8 *abuf, char *dst, int max) {
	int  i, a = *abuf;
	char c;

	for (i = 0;  (i < max) && (*(abuf++) == a); i++) {
		c = *(cbuf++);
		c = c & 0x7f;
		*(dst++) = (char)(c ? c : ' ');   /* copy and substitute zeros by spaces */
	}

	/* null termination */
	*dst = 0;
	return i;
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

/*** DRAW VTEXTSCREEN WIDGET ***/
static int vtextscr_draw(VTEXTSCREEN *vts, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	int fn_w = vts->vd->fn_w;
	int fn_h = vts->vd->fn_h;
	int alpha_offset = 0;
	int ret = 0;

	if (origin == vts) return 1;

	x += vts->wd->x;
	y += vts->wd->y;

	/* determine if terminal should be drawn in a transparent way */
	if (!vts->gen->get_mfocus((WIDGET *)vts) && config_transparency)
		alpha_offset = 3;

	/* sanity check */
	if (!vts->vd->buffer || !vts->vd->tmpstr) return ret;

	/* return if no transparency is used but origin is not the vtextscreen */
	if (!alpha_offset && origin && (origin != vts)) return ret;

	gfx->push_clipping(ds, x, y, vts->wd->w, vts->wd->h);

	if (alpha_offset)
		ret |= vts->gen->drawbehind(vts, vts, 0, 0, vts->wd->w, vts->wd->h, origin);

	if ((vts->vd->mode == VTEXTSCR_MODE_C8A8PLN)) {
		u8   *cbuf, *abuf;
		char *s = vts->vd->tmpstr;
		int   w = vts->vd->xres;
		int   h = vts->vd->yres;
		int   font_id = vts->vd->font_id;
		int   i, j, len;
		int   b;
		u32   bg, fg;
		
		/* draw text line */
		for (j = 0; j < h; j++) {
			cbuf = vts->vd->buffer + w*j;
			abuf = vts->vd->buffer + w*(h+j);
			
			for (i = 0; i < w;) {
				len = extract_substring(&cbuf[i], &abuf[i], s, w-i);
				
				/* set attibutes for this substring */
				b = (abuf[i]>>6) & 3;
				if (b>2) b = 2;
				bg = bg_coltab[b + alpha_offset][abuf[i] & 7];
				fg = fg_coltab[b][(abuf[i]>>3) & 7] | 0xff;

				/* draw substring */
				gfx->draw_box(ds, x + i*fn_w, y + j*fn_h, len*fn_w, fn_h, bg);
				gfx->draw_string(ds, x + i*fn_w, y + j*fn_h, fg, 0, font_id, s);

				i += len;
			}
		}
		ret |= 1;
	}

	/* draw cursor */
	if (ret || !alpha_offset) {
		u32 col = GFX_RGBA(255, 255, 255, 127);
		int cx = vts->vd->curs_x;
		int cy = vts->vd->curs_y;
		gfx->draw_box(ds, x + cx * fn_w, y + cy * fn_h, fn_w, fn_h, GFX_RGBA(64, 96, 127, 127));
		gfx->draw_hline(ds, x + cx * fn_w, y + cy * fn_h, fn_w, col);
		gfx->draw_hline(ds, x + cx * fn_w, y + (cy + 1) * fn_h - 1, fn_w, col);
		gfx->draw_vline(ds, x + cx * fn_w, y + cy * fn_h, fn_h, col);
		gfx->draw_vline(ds, x + (cx + 1) * fn_w - 1, y + cy * fn_h, fn_h, col);
		ret |= 1;
	}

	gfx->pop_clipping(ds);
	return ret;
}


static void vtextscr_refresh(VTEXTSCREEN *vts, s32 x, s32 y, s32 w, s32 h);

/*** WIDGET UPDATE ***/
static void (*orig_update) (WIDGET *);
static void vtextscr_update(VTEXTSCREEN *vts) {

	if (vts->vd->curs_x != vts->vd->curs_nx
	 || vts->vd->curs_y != vts->vd->curs_ny) {
		int curs_ox = vts->vd->curs_x;
		int curs_oy = vts->vd->curs_y;

		vts->vd->curs_x = vts->vd->curs_nx;
		vts->vd->curs_y = vts->vd->curs_ny;

		vtextscr_refresh(vts, vts->vd->curs_x, vts->vd->curs_y, 1, 1);
		vtextscr_refresh(vts, curs_ox, curs_oy, 1, 1);
	}

	orig_update(vts);
}


/*** FREE VTEXTSCREEN DATA ***/
static void vtextscr_free_data(VTEXTSCREEN *vts) {
  (void)vts;
	/* FIXME: free buffer with text representation */
}


/*** DETERMINE MIN/MAX SIZE OF VTEXTSCREEN ***/
static void vtextscr_calc_minmax(VTEXTSCREEN *vts) {
	vts->wd->min_w = vts->wd->max_w = vts->vd->xres * vts->vd->fn_w;
	vts->wd->min_h = vts->wd->max_h = vts->vd->yres * vts->vd->fn_h;
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *vtextscr_get_type(VTEXTSCREEN *vs) {
  (void)vs;
	return "VTextScreen";
}


/************************************
 *** VTEXTSCREEN SPECIFIC METHODS ***
 ************************************/

/*** TEST IF A GIVEN TEXT MODE IS VALID ***/
static s32 vtextscr_probe_mode(VTEXTSCREEN *vts, s32 width, s32 height, char *mode) {
  (void)vts;
	if (!dope_streq(mode, "C8A8PLN", 7)) return 0;
	if (width*height <= 0) return 0;
	return 1;
}


/*** SET TEXT MODE ***/
static s32 vtextscr_set_mode(VTEXTSCREEN *vts, s32 width, s32 height, char *mode) {
	int type = 0;

	if (!vtextscr_probe_mode(vts, width, height, mode)) return 0;

	/* destroy old buffer and reset values */
	if (vts->vd->smb)    shmem->destroy(vts->vd->smb);
	if (vts->vd->tmpstr) free(vts->vd->tmpstr);
	
	/* reset widget specific data */
	set_default_values(vts);

	/* create new buffer */
	if (dope_streq("C8A8PLN", mode, 7)) {
		type = VTEXTSCR_MODE_C8A8PLN;
		
		/* allocate shared memory buffer for text representation */
		vts->vd->smb = shmem->alloc(width * height * 2);

		/* allocate string buffer for conversion to gfx primitives */
		vts->vd->tmpstr = malloc(width + 3);
	}

	/* if anything went wrong free all resources and return */
	if (!type || !vts->vd->smb || !vts->vd->tmpstr) {
		
		ERROR(printf("VTextScreen(set_mode): mode %s not supported!\n", mode);)
		
		if (vts->vd->smb) shmem->destroy(vts->vd->smb);
		vts->vd->smb = NULL;
		if (vts->vd->tmpstr) free(vts->vd->tmpstr);
		vts->vd->tmpstr = NULL;
		return 0;
	}

	vts->vd->mode = type;
	vts->vd->xres = width;
	vts->vd->yres = height;
	vts->vd->buffer = shmem->get_address(vts->vd->smb);

	/* init attribute buffer to a white foreground and black background */
	if (type == VTEXTSCR_MODE_C8A8PLN) {
		memset(vts->vd->buffer, 0, width*height);
		memset(vts->vd->buffer + width*height, (7<<3) + (3<<6), width*height);
	}

	vts->wd->update |= WID_UPDATE_MINMAX;
	vts->gen->update(vts);
	return 1;
}


/*** UPDATE A SPECIFIED AREA OF TEXT ***/
static void vtextscr_refresh(VTEXTSCREEN *vts, s32 x, s32 y, s32 w, s32 h) {
	int sx1, sy1, sx2, sy2;

	if (w == -1) w = vts->vd->xres;
	if (h == -1) h = vts->vd->yres;
	if (w < 1 || h < 1) return;

	/* convert text position to pixel position */
	sx1 = vts->vd->fn_w*x;
	sy1 = vts->vd->fn_h*y;
	sx2 = vts->vd->fn_w*w + sx1 - 1;
	sy2 = vts->vd->fn_h*h + sy1 - 1;

	redraw->draw_widgetarea(vts, sx1, sy1, sx2, sy2);
}


/*** MAP VTEXTSCREEN BUFFER TO ANOTHER THREAD'S ADDRESS SPACE ***/
static char *vtextscr_map(VTEXTSCREEN *vts, char *dst_thread_ident) {
  (void)dst_thread_ident;
	s32 app_id;
	char dst_th_buf[16];
	THREAD *dst_th = (THREAD *)(void *)dst_th_buf;
	
	if (!vts->vd->smb) return "Error: VTextScreen mode not initialized.";

	/* if no valid thread identifier was suppied we map to the app's thread */
	/*if (thread->ident2thread(dst_thread_ident, dst_th))*/ {
		app_id = vts->gen->get_app_id(vts);
		dst_th = appman->get_app_thread(app_id);
	}
	shmem->share(vts->vd->smb, dst_th);
	shmem->get_ident(vts->vd->smb, &vts->vd->smb_ident[0]);
	INFO(printf("VTextScreen(map): return vts->vd->smb_ident = %s\n", &vts->vd->smb_ident[0]));
	return &vts->vd->smb_ident[0];
}


/*** GET WIDTH OF VTEXTSCREEN CHARACTERS ***/
static long vtextscr_get_char_w(VTEXTSCREEN *vts) {
	return vts->vd->fn_w;
}


/*** GET HEIGHT OF VTEXTSCREEN CHARACTERS ***/
static long vtextscr_get_char_h(VTEXTSCREEN *vts) {
	return vts->vd->fn_h;
}


/*** GET/SET X POSITION OF TEXT CURSOR ***/
static void vtextscr_set_cursor_x(VTEXTSCREEN *vts, long curs_x) {
	vts->vd->curs_nx = curs_x;
}
static long vtextscr_get_cursor_x(VTEXTSCREEN *vts) { return vts->vd->curs_x; }


/*** GET/SET Y POSITION OF TEXT CURSOR ***/
static void vtextscr_set_cursor_y(VTEXTSCREEN *vts, long curs_y) {
	vts->vd->curs_ny = curs_y;
}
static long vtextscr_get_cursor_y(VTEXTSCREEN *vts) { return vts->vd->curs_y; }


static struct widget_methods gen_methods;
static struct vtextscreen_methods vtextscreen_methods = {
	vtextscr_probe_mode,
	vtextscr_set_mode,
	vtextscr_map,
	vtextscr_refresh,
	NULL,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static VTEXTSCREEN *create(void) {

	VTEXTSCREEN *new = ALLOC_WIDGET(struct vtextscreen);
	SET_WIDGET_DEFAULTS(new, struct vtextscreen, &vtextscreen_methods);

	/* set widget type specific data */
	set_default_values(new);
	new->wd->flags |= WID_FLAGS_CONCEALING | WID_FLAGS_EDITABLE | WID_FLAGS_HIGHLIGHT;

	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct vtextscreen_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("VTextScreen", (void *(*)(void))create);

	script->reg_widget_method(widtype, "long probemode(long width,long height,string mode)", vtextscr_probe_mode);
	script->reg_widget_method(widtype, "long setmode(long width,long height,string mode)", vtextscr_set_mode);
	script->reg_widget_method(widtype, "string map(string thread=\"caller\")", vtextscr_map);
	script->reg_widget_method(widtype, "void refresh(long x=0, long y=0, long w=-1, long h=-1)", vtextscr_refresh);
	script->reg_widget_attrib(widtype, "long charw", vtextscr_get_char_w, NULL, NULL);
	script->reg_widget_attrib(widtype, "long charh", vtextscr_get_char_h, NULL, NULL);
	script->reg_widget_attrib(widtype, "long cursorx", vtextscr_get_cursor_x, vtextscr_set_cursor_x, gen_methods.update);
	script->reg_widget_attrib(widtype, "long cursory", vtextscr_get_cursor_y, vtextscr_set_cursor_y, gen_methods.update);

	widman->build_script_lang(widtype, &gen_methods);
}


int init_vtextscreen(struct dope_services *d) {
	int brightness;

	gfx     = d->get_module("Gfx 1.0");
	script  = d->get_module("Script 1.0");
	widman  = d->get_module("WidgetManager 1.0");
	redraw  = d->get_module("RedrawManager 1.0");
	appman  = d->get_module("ApplicationManager 1.0");
	thread  = d->get_module("Thread 1.0");
	fontman = d->get_module("FontManager 1.0");
	shmem   = d->get_module("SharedMemory 1.0");
	thread  = d->get_module("Thread 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_update = gen_methods.update;

	gen_methods.get_type    = vtextscr_get_type;
	gen_methods.draw        = vtextscr_draw;
	gen_methods.free_data   = vtextscr_free_data;
	gen_methods.calc_minmax = vtextscr_calc_minmax;
	gen_methods.update      = vtextscr_update;

	build_script_lang();

	/* generate color table */
	for (brightness = 0; brightness < 3; brightness++) {
		u32 r, g, b, i;
		for (i = 0; i < 8; i++) {

			/* get color components of color by its index */
			r = GFX_R(term_coltab[i]);
			g = GFX_G(term_coltab[i]);
			b = GFX_B(term_coltab[i]);

			/* apply brightness increment */
			r = r ? (r + brightness*50) : 0;
			g = g ? (g + brightness*50) : 0;
			b = b ? (b + brightness*50) : 0;

			fg_coltab[brightness][i]   = GFX_RGB(r, g, b);
			bg_coltab[brightness][i]   = GFX_RGBA(r, g, b, 255);
			bg_coltab[brightness + 3][i] = GFX_RGBA(r, g, b, 127);
		}
	}

	d->register_module("VTextScreen 1.0", &services);
	return 1;
}
