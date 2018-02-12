/*
 * \brief   DOpE Label widget module
 * \date    2002-05-15
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

struct label;
#define WIDGET struct label

#include "dopestd.h"
#include "gfx.h"
#include "widget_data.h"
#include "widget_help.h"
#include "variable.h"
#include "label.h"
#include "fontman.h"
#include "script.h"
#include "widman.h"

static struct widman_services  *widman;
static struct gfx_services     *gfx;
static struct fontman_services *font;
static struct script_services  *script;

struct label_data {
	char     *text;
	s16       font_id;
	s16       tx, ty;       /* text position inside the label cell */
	s16       pad_x, pad_y;
	VARIABLE *var;
};

int init_label(struct dope_services *d);

#define BLACK_SOLID GFX_RGBA(0, 0, 0, 255)


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

static void update_text_pos(LABEL *l) {
	if (!l->ld->text) return;
	l->ld->tx = (l->wd->w - font->calc_str_width (l->ld->font_id, l->ld->text))>>1;
	l->ld->ty = (l->wd->h - font->calc_str_height(l->ld->font_id, l->ld->text))>>1;
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static int lab_draw(LABEL *l, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	int tx = x + l->wd->x + l->ld->tx;
	int ty = y + l->wd->y + l->ld->ty;

	if (origin == l) return 1;
	if (origin) return 0;

	gfx->push_clipping(ds, x + l->wd->x, y + l->wd->y, l->wd->w, l->wd->h);
	if (l->ld->text) {
		gfx->draw_string(ds, tx, ty, BLACK_SOLID, 0, l->ld->font_id, l->ld->text);
	}
	gfx->pop_clipping(ds);

	return 1;
}


/*** DETERMINE MIN/MAX SIZE OF A LABEL WIDGET ***/
static void lab_calc_minmax(LABEL *l) {
	if (l->ld->text) {
		l->wd->min_w = l->wd->max_w = font->calc_str_width (l->ld->font_id, l->ld->text)
		                            + l->ld->pad_x * 2;
		l->wd->min_h = l->wd->max_h = font->calc_str_height(l->ld->font_id, l->ld->text)
		                            + l->ld->pad_y * 2;
	} else {
		l->wd->min_w = l->wd->min_h = l->ld->pad_x * 2;
		l->wd->max_w = l->wd->max_h = l->ld->pad_y * 2;
	}
}


static void (*orig_updatepos) (LABEL *l);
static void lab_updatepos(LABEL *l) {
	update_text_pos(l);
	orig_updatepos(l);
}


/*** FREE LABEL WIDGET DATA ***/
static void lab_free_data(LABEL *l) {
	if (l->ld->text) free(l->ld->text);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *lab_get_type(LABEL *l) {
  (void)l;
	return "Label";
}


/*******************************
 *** LABEL SPECIFIC METHODS ***
 *******************************/

static void lab_set_text(LABEL *l, char *new_txt) {
	if ((!l) || (!l->ld)) return;
	if (l->ld->text) free(l->ld->text);
	l->ld->text = dope_strdup(new_txt);
	l->wd->update |= WID_UPDATE_MINMAX;
}


static char *lab_get_text(LABEL *l) {
	return l->ld->text;
}


/*** SET FONT OF LABEL ***
 *
 * Currently, only the font identifiers 'default' and
 * 'monospaced' are defined.
 */
static void lab_set_font(LABEL *l, char *fontname) {
	if (dope_streq(fontname, "default", 255)) {
		l->ld->font_id = 0;
	} else if (dope_streq(fontname, "monospaced", 255)) {
		l->ld->font_id = 1;
	} else if (dope_streq(fontname, "big", 255)) {
		l->ld->font_id = 3;
	}
	l->wd->update |= WID_UPDATE_MINMAX;
}


/*** REQUEST CURRENTLY USED FONT OF A LABEL ***/
static char *lab_get_font(LABEL *l) {
	switch (l->ld->font_id) {
		case 1:  return "monospaced";
		case 3:  return "big";
		default: return "default";
	}
}


/*** CALLBACK ON VARIABLE MODIFICATIONS ***
 *
 * This routine is called everytime the variable get assigned
 * a new value. We just set the label text to the new value.
 */
static void lab_var_notify(LABEL *l, VARIABLE *v) {
	lab_set_text(l, v->var->get_string(v));
	l->gen->update(l);
}


/*** CONNECT LABEL TO A VARIABLE ***
 *
 * The label will always display the current value of the
 * variable.
 */
static void lab_set_var(LABEL *l, VARIABLE *v) {
	if (l->ld->var) {
		l->ld->var->gen->dec_ref((WIDGET *)l->ld->var);
		l->ld->var->var->disconnect(l->ld->var, l);
	}

	l->ld->var = v;
	if (v) {
		v->gen->inc_ref((WIDGET *)v);
		v->var->connect(v, l, lab_var_notify);
		lab_set_text(l, v->var->get_string(v));
	} else {
		lab_set_text(l, "");
	}
	l->wd->update |= WID_UPDATE_MINMAX;
}


static VARIABLE *lab_get_var(LABEL *l) {
	return l->ld->var;
}


static struct widget_methods gen_methods;
static struct label_methods lab_methods = {
	lab_set_text,
	lab_get_text,
	lab_set_font,
	lab_get_font,
	NULL,
	NULL,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static LABEL *create(void) {

	LABEL *new = ALLOC_WIDGET(struct label);
	SET_WIDGET_DEFAULTS(new, struct label, &lab_methods);

	/* set label specific attributes */
	new->ld->pad_x  = new->ld->pad_y = 2;
	new->ld->text   = dope_strdup("");
	update_text_pos(new);
	gen_methods.update(new);

	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct label_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("Label", (void *(*)(void))create);

	script->reg_widget_attrib(widtype, "string text", lab_get_text, lab_set_text, gen_methods.update);
	script->reg_widget_attrib(widtype, "string font", lab_get_font, lab_set_font, gen_methods.update);
	script->reg_widget_attrib(widtype, "Variable variable", lab_get_var, lab_set_var, gen_methods.update);

	widman->build_script_lang(widtype, &gen_methods);
}


int init_label(struct dope_services *d) {
	widman  = d->get_module("WidgetManager 1.0");
	gfx     = d->get_module("Gfx 1.0");
	font    = d->get_module("FontManager 1.0");
	script  = d->get_module("Script 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_updatepos = gen_methods.updatepos;

	gen_methods.draw        = lab_draw;
	gen_methods.updatepos   = lab_updatepos;
	gen_methods.get_type    = lab_get_type;
	gen_methods.calc_minmax = lab_calc_minmax;
	gen_methods.free_data   = lab_free_data;

	build_script_lang();

	d->register_module("Label 1.0", &services);
	return 1;
}
