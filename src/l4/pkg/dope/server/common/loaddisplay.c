/*
 * \brief   DOpE LoadDisplay widget module
 * \date    2004-01-09
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

struct loaddisplay;
#define WIDGET struct loaddisplay

#include "dopestd.h"
#include "loaddisplay.h"
#include "gfx_macros.h"
#include "widget_data.h"
#include "widget_help.h"
#include "script.h"
#include "widman.h"

static struct widman_services    *widman;
static struct gfx_services       *gfx;
static struct script_services    *script;

struct loadbar;
struct loadbar {
	char *ident;          /* identifier for bar                              */
	float value;          /* current value                                   */
	int   size;           /* size in pixels (depends on size of loaddisplay) */
	int   offset;         /* position relative to loaddisplay                */
	u32   color;          /* color                                           */
	struct loadbar *next; /* next bar of loaddisplay                         */
};

struct loaddisplay_data {
	u32 type;             /* bitmask of properties                       */
	s16 padx, pady;       /* space between loaddisplay and widget border */
	float from, to;       /* visible range of values                     */
	int colcnt;           /* counter for color assignment                */
	int zeropos;          /* pixel position of zero marker               */
	struct loadbar *bars; /* list of bars                                */
};

static GFX_CONTAINER *bg_img;
static GFX_CONTAINER *bar_img;

#define BLACK_SOLID GFX_RGBA(0, 0, 0, 255)
#define BLACK_MIXED GFX_RGBA(0, 0, 0, 127)
#define WHITE_SOLID GFX_RGBA(255, 255, 255, 255)
#define WHITE_MIXED GFX_RGBA(255, 255, 255, 127)

#define LOADDISPLAY_SIZE 16
#define LOADDISPLAY_UPDATE_ORIENT 0x01
#define LOADDISPLAY_UPDATE_VALUES 0x02

static u32 default_colors[] = {
	GFX_RGBA(255, 0, 0, 127), GFX_RGBA(0, 0, 255, 127),
	GFX_RGBA(0, 200, 0, 127), GFX_RGBA(200, 200, 0, 127)
};

#define NUM_COLORS (sizeof(default_colors)/4)

int init_loaddisplay(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

static inline void draw_raised_frame(GFX_CONTAINER *d, s32 x, s32 y, s32 w, s32 h) {
	gfx->draw_hline(d, x, y, w, WHITE_MIXED);
	gfx->draw_vline(d, x, y, h, WHITE_MIXED);
	gfx->draw_hline(d, x, y + h - 1, w, BLACK_SOLID);
	gfx->draw_vline(d, x + w - 1, y, h, BLACK_SOLID);
}


static inline void draw_pressed_frame(GFX_CONTAINER *d, s32 x, s32 y, s32 w, s32 h) {
	gfx->draw_hline(d, x, y, w, BLACK_SOLID);
	gfx->draw_vline(d, x, y, h, BLACK_SOLID);
	gfx->draw_hline(d, x, y + h - 1, w, WHITE_MIXED);
	gfx->draw_vline(d, x + w - 1, y, h, WHITE_MIXED);
}


/*** DRAW LOADBAR WITH SPECIFIED COLOR ***/
static inline void draw_bar(GFX_CONTAINER *ds, s32 x, s32 y, s32 w, s32 h, u32 color) {
	gfx->draw_img(ds, x, y, w, h, bar_img, 255);
	gfx->draw_box(ds, x + 1, y + 1, w - 2, h - 2, color);
	draw_raised_frame(ds, x, y, w, h);
}


/*** REFRESH PROPERTIES OF LOADDISPLAY THAT DEPEND ON CONFIGURABLE VALUES ***
 *
 * Attributes such as min/max w/h, the sizes of bars etc. depend
 * on the type of LoadDisplay (vertical/horizontal, fit or not).
 * This function keeps these properties consistent.
 */
static void refresh(LOADDISPLAY *ld) {
	float from = ld->ldd->from, to = ld->ldd->to, magnitude = to - from;
	float ppos, npos;   /* positive and negative position */
	float size;
	float *pos;
	int widsize;
	struct loadbar *lb = ld->ldd->bars;
	
	if (ld->ldd->type & LOADDISPLAY_VERTICAL) {
		widsize = ld->wd->h - 2*ld->ldd->pady - 4;
	} else {
		widsize = ld->wd->w - 2*ld->ldd->padx - 4;
	}

	/* calculate position of zero */
	ld->ldd->zeropos = ppos = npos = (widsize*(0 - from))/magnitude;
	
	/* leave space for zero marker if needed */
	if (from*to < 0) {
		ppos += (magnitude > 0) ? 2 : -2;
		npos -= (magnitude > 0) ? 2 : -2;
	}

	/* calculate size and positions of bars */
	while (lb) {
		size = (lb->value*widsize)/magnitude;
		pos  = (lb->value > 0) ? (&ppos) : (&npos);

		/* calculate pixel-size and pixel-offset of bar */
		if (lb->value == 0)
			lb->offset = lb->size = 0;
		else {
			lb->offset = *pos;
			lb->size = ((int)(*pos+size)) - lb->offset;
			if (!(ld->ldd->type & LOADDISPLAY_ABS)) *pos += size;
		}

		/* limit bars to visible range */
		if (lb->offset + lb->size > widsize) lb->size = widsize - lb->offset;
		if (lb->offset + lb->size < 0) lb->size = - lb->offset;
		if (lb->offset > widsize) lb->size = 0;
		if (lb->offset < 0) {
			lb->size += lb->offset;
			lb->offset = 0;
		}

		/* turn around bar with negative size */
		if (lb->size < 0) {
			lb->size = - lb->size;
			lb->offset = lb->offset - lb->size;
		}

		lb = lb->next;
	}
}


static struct loadbar *get_loadbar(struct loaddisplay *ld, char *ident) {
	struct loadbar *curr = ld->ldd->bars, *res = NULL;
	
	if (!ident) return NULL;
	
	/* search identifier in existing bars */
	while (curr) {
		if (dope_streq(ident, curr->ident, 256)) break;
		curr = curr->next;
	}
	
	if (curr) {
		return curr;

	/* no corresponding bar found -> create a new bar */
	} else {
		
		res = (struct loadbar *)zalloc(sizeof(struct loadbar));
		if (!res) {
			ERROR(printf("LoadDisplay(get_loadbar): out of memory!\n"));
			return NULL;
		}
		res->ident = dope_strdup(ident);
		res->color = default_colors[(ld->ldd->colcnt++) % NUM_COLORS];

		/* append new bar to list */ 
		curr = ld->ldd->bars;
		if (curr) {
			while (curr->next) curr = curr->next;
			curr->next = res;
		} else {
			ld->ldd->bars = res;
		}
		return res;
	}
}


/*** CONVERT TWO-DIGIT HEX NUMBER TO A UNSIGNED CHAR ***/
static inline u8 hex2u8(const char *s) {
	int i;
	u32 result = 0;
	for (i = 0; i < 2; i++, s++) {
		if (!(*s)) return result;
		result = result*16 + (*s & 0xf);
		if (*s > '9') result += 9;
	}
	return result;
}


/*** STATIC MAPPING FROM COLORNAMES TO 32BIT RGB VALUES ***/

#define COLMAP_SIZE 3
struct {
	char *name;
	u32   value;
} colmap[COLMAP_SIZE] = {
	{"red",   GFX_RGB(255, 0, 0)},
	{"green", GFX_RGB(0, 255, 0)},
	{"blue",  GFX_RGB(0, 0, 255)},
};


/*** RETURN 32BIT COLOR VALUE OF A COLOR WITH THE GIVEN NAME ***
 *
 * Currently the syntax #rrggbb is supported only. If this
 * functionality is needed also by other DOpE components,
 * we could put it into a separate DOpE component.
 */
static u32 get_color_by_name(const char *colname) {
	int i;
	if (!colname) return 0;
	
	if ((*colname == '#') && (strlen(colname) >= 7))
		return GFX_RGB(hex2u8(colname + 1), hex2u8(colname + 3), hex2u8(colname + 5));
	
	/* check for predefined color names */
	for (i = 0; i < COLMAP_SIZE; i++) {
		if (dope_streq(colmap[i].name, colname, 255)) return colmap[i].value;
	}

	/* color is unknown */
	return 0;
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static int ld_draw(LOADDISPLAY *ld, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	struct loadbar *lb = ld->ldd->bars;
	int w = ld->wd->w - 2*ld->ldd->padx;
	int h = ld->wd->h - 2*ld->ldd->pady;
	x    += ld->wd->x + ld->ldd->padx;
	y    += ld->wd->y + ld->ldd->pady;

	if (origin == ld) return 1;
	if (origin) return 0;

	gfx->push_clipping(ds, x, y, w, h);
	gfx->draw_img(ds, x, y, w, h, bg_img, 255);

	if (ld->ldd->from * ld->ldd->to >= 0)
		draw_pressed_frame(ds, x, y, w, h);

	if (ld->ldd->type & LOADDISPLAY_VERTICAL) {
		/* zero marker */
		if (ld->ldd->from * ld->ldd->to < 0) {
			draw_pressed_frame(ds, x, y, w, ld->ldd->zeropos + 2);
			draw_pressed_frame(ds, x, y+ld->ldd->zeropos + 2, w, h-ld->ldd->zeropos - 2);
		}
		while (lb) {
			if (lb->size > 0)
				draw_bar(ds, x + 2, y + 2 + lb->offset, w - 4, lb->size, lb->color);
			lb = lb->next;
		}
	} else {
		/* zero marker */
		if (ld->ldd->from * ld->ldd->to < 0) {
			draw_pressed_frame(ds, x, y, ld->ldd->zeropos + 2, h);
			draw_pressed_frame(ds, x+ld->ldd->zeropos + 2, y, w - ld->ldd->zeropos - 2, h);
		}
		while (lb) {
			if (lb->size > 0)
				draw_bar(ds, x + 2 + lb->offset, y + 2, lb->size, h - 4, lb->color);
			lb = lb->next;
		}
	}

	gfx->pop_clipping(ds);
	return 1;
}


/*** DETERMINE MIN/MAX SIZE OF LOADDISPLAY WIDGET ***/
static void ld_calc_minmax(LOADDISPLAY *ld) {
	if (ld->ldd->type & LOADDISPLAY_VERTICAL) {
		ld->wd->max_w = LOADDISPLAY_SIZE;
		ld->wd->max_h = 99999;
	} else {
		ld->wd->max_w = 99999;
		ld->wd->max_h = LOADDISPLAY_SIZE;
	}
}


/*** UPDATE LOADDISPLAY AFTER CHANGE OF ATTRIBUTES ***/
static void (*orig_updatepos)(WIDGET *w);
static void ld_updatepos(LOADDISPLAY *ld) {
	refresh(ld);
	orig_updatepos(ld);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *ld_get_type(LOADDISPLAY *ld) {
  (void)ld;
	return "LoadDisplay";
}

static struct widget_methods gen_methods;


/************************************
 *** LOADDISPLAY SPECIFIC METHODS ***
 ************************************/

/*** GET/SET MIN LOADDISPLAY VALUE ***/
static void ld_set_from(LOADDISPLAY *ld, float new_from) {
	ld->ldd->from = new_from;
	ld->wd->update |= WID_UPDATE_REFRESH;
}
static float ld_get_from(LOADDISPLAY *ld) {
	return ld->ldd->from;
}


/*** GET/SET MAX VALUE ***/
static void ld_set_to(LOADDISPLAY *ld, float new_to) {
	ld->ldd->to = new_to;
	ld->wd->update |= WID_UPDATE_REFRESH;
}
static float ld_get_to(LOADDISPLAY *ld) {
	return ld->ldd->to;
}


static void ld_set_orient(LOADDISPLAY *ld, char *orient) {
	
	if (!strcmp("vertical", orient))
		ld->ldd->type |= LOADDISPLAY_VERTICAL;
	
	if (!strcmp("horizontal", orient))
		ld->ldd->type &= ~LOADDISPLAY_VERTICAL;

	ld->wd->update |= WID_UPDATE_MINMAX;
}


static char *ld_get_orient(LOADDISPLAY *ld) {
	if (ld->ldd->type & LOADDISPLAY_VERTICAL) return "vertical";
	else return "horizontal";
}


static void ld_barconfig(LOADDISPLAY *ld, char *ident, char *value, char *color) {
	struct loadbar *lb;
	
	if (!ident || !value) return;
	if (!(lb = get_loadbar(ld, ident))) return;

	if (!dope_streq("<none>", value, 7)) {
		lb->value = strtod(value, (char **)NULL);
	}

	if (!dope_streq("<default>", color, 9)) {
		u32 c = get_color_by_name(color);

		/* set alpha to 50% */
		lb->color = GFX_RGBA(GFX_R(c), GFX_G(c), GFX_B(c), 127);
	}

	refresh(ld);
	ld->wd->update |= WID_UPDATE_REFRESH;
	gen_methods.update((WIDGET *)ld);
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static struct loaddisplay_methods ld_methods = {
	ld_set_orient,
	ld_set_from,
	ld_set_to,
	ld_barconfig,
};


static LOADDISPLAY *create(void) {

	LOADDISPLAY *new = ALLOC_WIDGET(struct loaddisplay);
	SET_WIDGET_DEFAULTS(new, struct loaddisplay, &ld_methods);

	new->wd->min_w = new->wd->min_h = new->wd->max_h = LOADDISPLAY_SIZE;

	/* set loaddisplay specific attributes */
	new->ldd->padx    = 2;
	new->ldd->pady    = 2;
	new->ldd->from    = 0.0;
	new->ldd->to      = 100.0;
	new->ldd->bars    = NULL;
	new->ldd->type    = 0;
	new->ldd->zeropos = 0;
	new->ldd->colcnt  = 0;

	/* refresh config dependent attributes */
	refresh(new);
	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct loaddisplay_services services = {
	create
};


static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("LoadDisplay", (void *(*)(void))create);

	script->reg_widget_method(widtype, "void barconfig(string ident,string value=\"<none>\",string color=\"<default>\")", &ld_barconfig);
	script->reg_widget_attrib(widtype, "string orient", ld_get_orient, ld_set_orient, gen_methods.update);
	script->reg_widget_attrib(widtype, "float from", ld_get_from, ld_set_from, gen_methods.update);
	script->reg_widget_attrib(widtype, "float to", ld_get_to, ld_set_to, gen_methods.update);

	widman->build_script_lang(widtype, &gen_methods);
}



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_loaddisplay(struct dope_services *d) {

	widman = d->get_module("WidgetManager 1.0");
	gfx    = d->get_module("Gfx 1.0");
	script = d->get_module("Script 1.0");

	bg_img  = gen_range_img(gfx, 128, 128, 128, 50, 50, 50);
	bar_img = gen_range_img(gfx, -256, -256, -256, 255, 255, 255);

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_updatepos = gen_methods.updatepos;
	
	gen_methods.get_type    = ld_get_type;
	gen_methods.draw        = ld_draw;
	gen_methods.calc_minmax = ld_calc_minmax;
	gen_methods.updatepos   = ld_updatepos;

	build_script_lang();

	d->register_module("LoadDisplay 1.0", &services);
	return 1;
}
