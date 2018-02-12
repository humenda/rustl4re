/*
 * \brief   DOpE Background widget module
 * \date    2002-11-13
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

struct background;
#define WIDGET struct background

#include "dopestd.h"
#include "background.h"
#include "gfx.h"
#include "widget_data.h"
#include "widget_help.h"
#include "widman.h"
#include "userstate.h"
#include "script.h"

static struct gfx_services *gfx;
static struct widman_services *widman;
static struct script_services *script;
static struct userstate_services *userstate;

struct background_data {
	long    style;
	void  (*click) (void *);
	WIDGET *content;
};

int init_background(struct dope_services *d);


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static int bg_draw(BACKGROUND *b, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	WIDGET *c;
	int ret = 0;

	if (origin == b) return 1;
	
	c = b->bd->content;

	x += b->wd->x;
	y += b->wd->y;

	if (!origin) switch (b->bd->style) {
		case BG_STYLE_WIN:
			gfx->draw_box(ds, x, y, b->wd->w, b->wd->h, 0x708090ff);
			ret |= 1;
			break;
		case BG_STYLE_DESK:
			gfx->draw_box(ds, x, y, b->wd->w, b->wd->h, 0x506070ff);
			ret |= 1;
			break;
	}
	if (c) ret |= c->gen->draw(c, ds, x, y, origin);
	return ret;
}


static WIDGET *bg_find(BACKGROUND *b, long x, long y) {
	WIDGET *c, *result = b;
	if (!b) return NULL;

	/* return if position is outside the background area */
	if ((x < b->wd->x) && (x >= b->wd->x + b->wd->w)
	 && (y < b->wd->y) && (y >= b->wd->y + b->wd->h))
		return NULL;

	if ((c = b->bd->content))
		result = c->gen->find(c, x, y);
		
	return result;
}


static void (*orig_handle_event) (BACKGROUND *b, EVENT *e, WIDGET *from);
static void bg_handle_event(BACKGROUND *b, EVENT *e, WIDGET *from) {
	if ((e->type == L4RE_EV_KEY && e->value == 1) && (b->bd->click)) {
		b->bd->click(b);
		return;
	}
	if (orig_handle_event) orig_handle_event(b, e, from);
}


static void bg_calc_minmax(BACKGROUND *b) {
	WIDGET *c = b->bd->content;

	if (c) {
		b->wd->min_w = c->wd->min_w;
		b->wd->min_h = c->wd->min_h;
		b->wd->max_w = c->wd->max_w;
		b->wd->max_h = c->wd->max_h;
	} else {
		b->wd->min_w = b->wd->min_h = 0;
		b->wd->max_w = b->wd->max_h = 9999;
	}
}


/*** UPDATE POSITION AND SIZE OF BACKGROUND ***
 *
 * Keep size of child in sync with background.
 */
static void (*orig_updatepos)(WIDGET *w);
static void bg_updatepos(BACKGROUND *b) {
	WIDGET *c = b->bd->content;
	if (c) {
		c->gen->set_x(c, 0);
		c->gen->set_y(c, 0);
		c->gen->set_w(c, b->wd->w);
		c->gen->set_h(c, b->wd->h);
		c->gen->updatepos(c);
	}
	orig_updatepos(b);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *bg_get_type(BACKGROUND *bg) {
    (void)bg;
	return "Background";
}


/*** REMOVE CONTENT FROM THE BACKGROUND ***/
static void bg_remove_child(BACKGROUND *b, WIDGET *child) {
	if (!child || (b->bd->content != child)) return;

	b->bd->content = NULL;
	child->gen->set_parent(child, NULL);
	child->gen->dec_ref(child);

	b->wd->update |= WID_UPDATE_MINMAX;
	b->gen->update(b);
}


/*** FREE DATA ***/
static void bg_free_data(BACKGROUND *b) {

	/* get rid of the content */
	if (b->bd->content)
		b->bd->content->gen->release(b->bd->content);
}


/***********************************
 *** BACKGROUND SPECIFIC METHODS ***
 ***********************************/

static void bg_set_style(BACKGROUND *b, long new_style) {
	b->bd->style = new_style;
}


static void bg_set_click(BACKGROUND *b, void (*click)(void *)) {
	b->bd->click = click;
}


static void bg_set_content(BACKGROUND *b, WIDGET *new_content) {

	/* avoid cyclic parent relationships */
	if (new_content->gen->related_to(new_content, b)) return;

	/* get rid of old content */
	if (b->bd->content) {
		b->bd->content->gen->set_parent(b->bd->content, NULL);
		b->bd->content->gen->dec_ref(b->bd->content);
	}

	/* welcome new content */
	b->bd->content = new_content;
	if (new_content) {
		new_content->gen->inc_ref(new_content);
		new_content->gen->release(new_content);
		new_content->gen->set_parent(new_content, b);
	}
	
	b->wd->update |= WID_UPDATE_MINMAX;
}


static struct widget_methods gen_methods;
static struct background_methods bg_methods = {
	bg_set_style,
	bg_set_click,
};



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static BACKGROUND *create(void) {

	BACKGROUND *new = ALLOC_WIDGET(struct background);
	SET_WIDGET_DEFAULTS(new, struct background, &bg_methods);

	/* set background specific default attributes */
	new->bd->style  =  BG_STYLE_WIN;
	new->wd->flags |=  WID_FLAGS_CONCEALING;

	return new;
}



/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct background_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("Background", (void *(*)(void))create);
	script->reg_widget_attrib(widtype, "Widget content", NULL, bg_set_content, gen_methods.update);
	widman->build_script_lang(widtype, &gen_methods);
}


int init_background(struct dope_services *d) {

	gfx       = d->get_module("Gfx 1.0");
	widman    = d->get_module("WidgetManager 1.0");
	userstate = d->get_module("UserState 1.0");
	script    = d->get_module("Script 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);
	orig_handle_event        = gen_methods.handle_event;
	orig_updatepos           = gen_methods.updatepos;
	
	gen_methods.get_type     = bg_get_type;
	gen_methods.draw         = bg_draw;
	gen_methods.find         = bg_find;
	gen_methods.handle_event = bg_handle_event;
	gen_methods.updatepos    = bg_updatepos;
	gen_methods.calc_minmax  = bg_calc_minmax;
	gen_methods.remove_child = bg_remove_child;
	gen_methods.free_data    = bg_free_data;

	build_script_lang();

	d->register_module("Background 1.0", &services);
	return 1;
}






