/*
 * \brief   DOpE Scrollbar widget module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This widget type handles scrollbars. It uses
 * Buttons as child widgets.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

struct scrollbar;
#define WIDGET struct scrollbar

#include "dopestd.h"
#include "button.h"
#include "widget_data.h"
#include "widget_help.h"
#include "widman.h"
#include "scrollbar.h"
#include "userstate.h"
#include "script.h"
#include "messenger.h"

static struct widman_services    *widman;
static struct button_services    *but;
static struct script_services    *script;
static struct messenger_services *msg;
static struct userstate_services *userstate;

#define SCROLLBAR_NUM_ELEM 4  /* number of scrollbar element widgets */
#define SB_ELEM_INC_ARROW  0  /* right/bottom arrow                  */
#define SB_ELEM_DEC_ARROW  1  /* left/top arrow                      */
#define SB_ELEM_SLIDER_FG  2  /* slider                              */
#define SB_ELEM_SLIDER_BG  3  /* background of the slider            */

#define SCROLLBAR_VER      0x1
#define SCROLLBAR_AUTOVIEW 0x2

struct scrollbar_data {
	u32     type;            /* bitmask of scrollbars properties         */
	s16     arrow_size;      /* width or height of an arrow-element      */
	s32     real_size;       /* size of the real contents                */
	s32     view_size;       /* size of the visible part of the contents */
	s32     view_offset;     /* position of the visible part             */
	s16     step_size;       /* step movement when arrow is pressed      */
	void  (*scroll_update) (void *);  /* scroll update callback function */
	WIDGET *elem[SCROLLBAR_NUM_ELEM]; /* scrollbar elements              */
	void   *scroll_update_arg;
};


int init_scrollbar(struct dope_services *d);


/*********************************
 *** INTERNALLY USED FUNCTIONS ***
 *********************************/

static u32 calc_slider_pos(u32 real_size, u32 view_offs, u32 slid_bg_size) {
	if (real_size == 0) return 0;
	return (view_offs * slid_bg_size) / real_size;
}


static u32 calc_slider_offset(u32 real_size, u32 slid_pos, u32 slid_bg_size) {
	if (slid_bg_size == 0) return 0;
	return (slid_pos * real_size) / slid_bg_size;
}


static u32 calc_slider_size(u32 real_size, u32 view_size, u32 slid_bg_size) {
	u32 result;
	if (real_size == 0) return 0;
	result = (view_size * slid_bg_size) / real_size;
	if (result < slid_bg_size) return result;
	return slid_bg_size;
}


/*** SET SIZE AND POSITION OF A WIDGET AND UPDATE IT ***/
static inline void set_xywh(WIDGET *cw, int x, int y, int w, int h) {
	cw->gen->set_x(cw, x);
	cw->gen->set_y(cw, y);
	cw->gen->set_w(cw, w);
	cw->gen->set_h(cw, h);
	cw->gen->updatepos(cw);
}


/*** SET SIZES AND POSITIONS OF SCROLLBAR CONTROL ELEMENTS ***/
static void refresh_elements(SCROLLBAR *s) {
	int as  = s->sd->arrow_size;
	int w   = s->wd->w;
	int h   = s->wd->h;
	int sbg = ((s->sd->type & SCROLLBAR_VER) ? h : w) - 2*as;

	if (s->sd->type & SCROLLBAR_AUTOVIEW)
		s->sd->view_size = (s->sd->type & SCROLLBAR_VER) ? h : w;

	/* adapt view offset if necessary */
	s->scroll->set_view_offset(s, s->sd->view_offset);

	if (s->sd->type & SCROLLBAR_VER) {
		set_xywh(s->sd->elem[SB_ELEM_INC_ARROW], 0, h - as, w, as);
		set_xywh(s->sd->elem[SB_ELEM_DEC_ARROW], 0, 0, w, as);
		set_xywh(s->sd->elem[SB_ELEM_SLIDER_BG], 0, as, w, sbg);
		set_xywh(s->sd->elem[SB_ELEM_SLIDER_FG], 2, as + 2
		       + calc_slider_pos(s->sd->real_size, s->sd->view_offset, sbg - 3),
		         w - 2*2,
		         calc_slider_size(s->sd->real_size, s->sd->view_size, sbg - 3));
	} else {
		set_xywh(s->sd->elem[SB_ELEM_INC_ARROW], w - as, 0, as, h);
		set_xywh(s->sd->elem[SB_ELEM_DEC_ARROW], 0, 0, as, h);
		set_xywh(s->sd->elem[SB_ELEM_SLIDER_BG], as, 0, sbg, h);
		set_xywh(s->sd->elem[SB_ELEM_SLIDER_FG], as + 2
		       + calc_slider_pos(s->sd->real_size, s->sd->view_offset, sbg - 3), 2,
		         calc_slider_size(s->sd->real_size, s->sd->view_size, sbg - 3),
		         h - 2*2);
	}
}


/***********************************
 *** USERSTATE HANDLER FUNCTIONS ***
 ***********************************/

/*** VARIABLES FOR USERSTATE HANDLING ***/
static int        osx, osy;              /* slider position when begin of dragging */
static SCROLLBAR *curr_scrollbar;        /* currently modified scrollbar widget    */
static float      curr_scroll_speed;     /* current scroll speed                   */
static float      max_scroll_speed = 20; /* maximal scroll speed                   */
static float      scroll_accel = 0.4;    /* scroll acceleration                    */

static void slider_motion_callback(WIDGET *w, int dx, int dy) {
  (void)w;
	curr_scrollbar->scroll->set_slider_x(curr_scrollbar, osx + dx);
	curr_scrollbar->scroll->set_slider_y(curr_scrollbar, osy + dy);
	refresh_elements(curr_scrollbar);
	if (curr_scrollbar->sd->scroll_update)
		curr_scrollbar->sd->scroll_update(curr_scrollbar->sd->scroll_update_arg);
	curr_scrollbar->gen->update(curr_scrollbar);
}


static void scrollstep_tick_callback(WIDGET *w, int dx, int dy) {
  (void)dx; (void)dy;
	s32 offset, d, mb;

	if (!curr_scrollbar) return;

	mb = userstate->get_mb();
	if (!mb) return;

	offset = curr_scrollbar->scroll->get_view_offset(curr_scrollbar);
	d      = (s32)curr_scroll_speed;

	/* check if left or right arrow is selected */
	if (w->gen->get_context(w)) d = -d;

	/* check if left or right mouse button is pressed */
	if (mb == 2) d = -d;

	curr_scrollbar->scroll->set_view_offset(curr_scrollbar, offset + d);
	refresh_elements(curr_scrollbar);
	if (curr_scrollbar->sd->scroll_update)
		curr_scrollbar->sd->scroll_update(curr_scrollbar->sd->scroll_update_arg);
	curr_scrollbar->gen->force_redraw(curr_scrollbar);

	curr_scroll_speed += scroll_accel;
	if (curr_scroll_speed > max_scroll_speed) curr_scroll_speed = max_scroll_speed;
}


static void scrollstep_leave_callback(WIDGET *cw, int dx, int dy) {
  (void)dx; (void)dy;
	cw->gen->set_state(cw, 0);
	cw->gen->update(cw);
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static int scrollbar_draw(SCROLLBAR *w, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	int ret = 0, i;

	if (origin == w) return 1;

	/* draw elements of the scrollbar */
	for (i = 0; i < SCROLLBAR_NUM_ELEM; i++) {

		/* get element widget, begin with slider background */
		WIDGET *cw = w->sd->elem[(i + SB_ELEM_SLIDER_BG) % SCROLLBAR_NUM_ELEM];
		ret |= cw->gen->draw(cw, ds, w->wd->x + x, w->wd->y + y, origin);
	}
	return ret;
}


/*** CALCULATE MINIMAL AND MAXIMAL SIZE OF SCROLLBAR ***/
static void scrollbar_calc_minmax(SCROLLBAR *s) {

	if (s->sd->type & SCROLLBAR_VER) {
		s->wd->min_w = s->wd->max_w = s->sd->arrow_size;
		s->wd->min_h = 2 * s->sd->arrow_size;
		s->wd->max_h = 9999;
	} else {
		s->wd->min_w = 2 * s->sd->arrow_size;
		s->wd->max_w = 9999;
		s->wd->min_h = s->wd->max_h = s->sd->arrow_size;
	}
}


static void (*orig_scroll_updatepos) (WIDGET *w);
static void scrollbar_updatepos(SCROLLBAR *w) {
	refresh_elements(w);
	orig_scroll_updatepos(w);

	/* refresh the widget each time an attribute changed */
	w->wd->update |= WID_UPDATE_REFRESH;
}


static WIDGET *scrollbar_find(SCROLLBAR *w, long x, long y) {
	WIDGET *ret;
	int i;

	/* check if position is not inside the scrollbar */
	if ((x < w->wd->x) || (x >= w->wd->x + w->wd->w)
	 || (y < w->wd->y) || (y >= w->wd->y + w->wd->h)) return NULL;

	/* we are hit - lets check our children */
	for (i = 0; i < SCROLLBAR_NUM_ELEM; i++) {
		ret = w->sd->elem[i]->gen->find(w->sd->elem[i], x - w->wd->x, y - w->wd->y);
		if (ret) return ret;
	}
	return NULL;
}


/*** FREE SCROLLBAR WIDGET DATA ***/
static void scrollbar_free_data(SCROLLBAR *sb) {
	int i;
	for (i = 0; i < SCROLLBAR_NUM_ELEM; i++)
		if (sb->sd->elem[i])
			sb->sd->elem[i]->gen->dec_ref(sb->sd->elem[i]);
}


/**********************************
 *** SCROLLBAR SPECIFIC METHODS ***
 **********************************/

/*** GET/SET Y POSITION OF SLIDER ***/
static void scrollbar_set_slider_x (SCROLLBAR *s, s32 new_sx) {
	WIDGET *cw;
	u32 offset;

	/* ignore x slider position change of vertical scrollbar */
	if (s->sd->type & SCROLLBAR_VER) return;

	new_sx -= s->sd->arrow_size - 2;
	if (new_sx < 0) new_sx = 0;

	cw = s->sd->elem[SB_ELEM_SLIDER_BG];
	offset = calc_slider_offset(s->sd->real_size, new_sx, cw->gen->get_w(cw) - 3);
	s->scroll->set_view_offset(s, offset);
	s->wd->update |= WID_UPDATE_REFRESH;
}
static u32 scrollbar_get_slider_x (SCROLLBAR *s) {
	WIDGET *cw = s->sd->elem[SB_ELEM_SLIDER_FG];
	return cw->gen->get_x(cw);
}


/*** GET/SET Y POSITION OF SLIDER ***/
static void scrollbar_set_slider_y (SCROLLBAR *s, s32 new_sy) {
	WIDGET *cw;
	u32 offset;

	/* ignore y slider position change of horizontal scrollbar */
	if (!(s->sd->type & SCROLLBAR_VER)) return;

	new_sy -= s->sd->arrow_size - 2;
	if (new_sy < 0) new_sy = 0;

	cw = s->sd->elem[SB_ELEM_SLIDER_BG];
	offset = calc_slider_offset(s->sd->real_size, new_sy, cw->gen->get_h(cw) - 3);
	s->scroll->set_view_offset(s, offset);
	s->wd->update |= WID_UPDATE_REFRESH;
}
static u32 scrollbar_get_slider_y (SCROLLBAR *s) {
	WIDGET *cw = s->sd->elem[SB_ELEM_SLIDER_FG];
	return cw->gen->get_y(cw);
}


/*** GET/SET REAL DIMENSION OF SCROLL AREA ***/
static void scrollbar_set_real_size (SCROLLBAR *s, u32 new_real_size) {
	s->sd->real_size = new_real_size;
	s->wd->update |= WID_UPDATE_REFRESH;
}
static u32 scrollbar_get_real_size (SCROLLBAR *s) {
	return s->sd->real_size;
}


/*** GET/SET VISIBLE SIZE OF SCROLL AREA ***/
static void scrollbar_set_view_size (SCROLLBAR *s, u32 new_view_size) {
	s->sd->view_size = new_view_size;
	s->wd->update |= WID_UPDATE_REFRESH;
}
static u32 scrollbar_get_view_size (SCROLLBAR *s) {
	return s->sd->view_size;
}


/*** GET/SET VIEW OFFSET OF SCROLL AREA ***/
static void scrollbar_set_view_offset (SCROLLBAR *s, s32 new_view_offset) {

	if (new_view_offset > s->sd->real_size - s->sd->view_size)
		new_view_offset = s->sd->real_size - s->sd->view_size;
	if (new_view_offset < 0) new_view_offset = 0;
	if (new_view_offset == s->sd->view_offset) return;

	s->sd->view_offset = new_view_offset;

	/* send change event to the application */
	{
		unsigned long message = s->gen->get_bind_msg(s, "change");
		int   app_id  = s->gen->get_app_id(s);
		if (message) msg->send_action_event(app_id, "change", message);
	}
	s->wd->update |= WID_UPDATE_REFRESH;
}
static u32 scrollbar_get_view_offset (SCROLLBAR *s) {
	return s->sd->view_offset;
}


static s32 scrollbar_get_arrow_size(SCROLLBAR *s) {
	return s->sd->arrow_size;
}


static void scrollbar_reg_scroll_update(SCROLLBAR *s, void (*callback)(void *), void *arg) {
	s->sd->scroll_update = callback;
	s->sd->scroll_update_arg = arg;
}


static void scrollbar_set_orient(SCROLLBAR *s, char *orient) {
	if (!strcmp("vertical", orient))
		s->sd->type |= SCROLLBAR_VER;
	else if (!strcmp("horizontal", orient))
		s->sd->type &= ~SCROLLBAR_VER;
	s->wd->update |= WID_UPDATE_MINMAX;
}


static char *scrollbar_get_orient(SCROLLBAR *s) {
	if (s->sd->type & SCROLLBAR_VER) return "vertical";
	return "horizontal";
}


/*** GET/SET AUTOVIEW ATTRIBUTE ***/
static void scrollbar_set_autoview(SCROLLBAR *s, long av_flag) {
	s->sd->type = av_flag ? (s->sd->type |  SCROLLBAR_AUTOVIEW)
	                      : (s->sd->type & ~SCROLLBAR_AUTOVIEW);
}
static long scrollbar_get_autoview(SCROLLBAR *s) {
	return !!(s->sd->type & SCROLLBAR_AUTOVIEW);
}


static struct widget_methods gen_methods;
static struct scrollbar_methods scroll_methods = {
	scrollbar_set_orient,
	scrollbar_get_orient,
	scrollbar_set_autoview,
	scrollbar_get_autoview,
	scrollbar_set_slider_x,
	scrollbar_get_slider_x,
	scrollbar_set_slider_y,
	scrollbar_get_slider_y,
	scrollbar_set_real_size,
	scrollbar_get_real_size,
	scrollbar_set_view_size,
	scrollbar_get_view_size,
	scrollbar_set_view_offset,
	scrollbar_get_view_offset,
	scrollbar_get_arrow_size,
	scrollbar_reg_scroll_update,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static WIDGET *new_button(WIDGET *parent, char *txt, void *clic, long context, int state) {
	BUTTON *nb = but->create();
	nb->but->set_click(nb, clic);
	nb->gen->set_context((WIDGET *)nb, (void *)context);
	nb->but->set_text(nb, txt);
	nb->gen->set_evforward((WIDGET *)nb, 0);
	nb->gen->set_selectable((WIDGET *)nb, 0);
	nb->but->set_pad_x(nb, 0);
	nb->but->set_pad_y(nb, 0);
	nb->gen->set_parent((WIDGET *)nb, parent);
	nb->gen->set_state((WIDGET *)nb, state);
	return (WIDGET *)nb;
}


static void slider_callback(BUTTON *b) {
	if (!b || !(curr_scrollbar = b->gen->get_parent((WIDGET *)b))) return;

	osx = curr_scrollbar->scroll->get_slider_x(curr_scrollbar);
	osy = curr_scrollbar->scroll->get_slider_y(curr_scrollbar);
	userstate->drag((WIDGET *)b, slider_motion_callback, NULL, NULL);
}


static void slidbg_callback(BUTTON *b) {
	SCROLLBAR  *s;
	WIDGET     *cw;
	int dir, offset;

	if (!b || !(s = (SCROLLBAR *)b->gen->get_parent((WIDGET *)b))) return;

	/* check if mouse is on the left or right side of the slider */
	cw = s->sd->elem[SB_ELEM_SLIDER_FG];
	if (s->sd->type & SCROLLBAR_VER) {
		dir = (userstate->get_my() < cw->gen->get_abs_y(cw)) ? 0 : 1;
	} else {
		dir = (userstate->get_mx() < cw->gen->get_abs_x(cw)) ? 0 : 1;
	}

	/* change view offset dependent on the scroll direction */
	offset = s->sd->view_offset + (dir ? 1 : -1) * s->sd->view_size;
	s->scroll->set_view_offset(s, offset);

	refresh_elements(s);
	if (s->sd->scroll_update)
		s->sd->scroll_update(s->sd->scroll_update_arg);
	s->gen->update(s);
}


static void arrow_callback(BUTTON *b) {
	if (!b || !(curr_scrollbar = b->gen->get_parent((WIDGET *)b))) return;

	b->gen->set_state((WIDGET *)b, 1);
	b->gen->update((WIDGET *)b);
	curr_scroll_speed = 0.6;
	userstate->drag((WIDGET *)b, NULL, scrollstep_tick_callback, scrollstep_leave_callback);
}


static SCROLLBAR *create(void) {

	SCROLLBAR *new = ALLOC_WIDGET(struct scrollbar);
	SET_WIDGET_DEFAULTS(new, struct scrollbar, &scroll_methods);

	/* default scrollbar attributes */
	new->sd->type          = SCROLLBAR_AUTOVIEW;
	new->sd->arrow_size    = 13;
	new->sd->real_size     = 500;
	new->sd->view_size     = 128;
	new->sd->view_offset   = 0;
	new->sd->step_size     = 16;
	new->sd->scroll_update = NULL;

	/* create scrollbar element widgets */
	new->sd->elem[SB_ELEM_INC_ARROW] = new_button(new, NULL, arrow_callback,  0, 0);
	new->sd->elem[SB_ELEM_DEC_ARROW] = new_button(new, NULL, arrow_callback,  1, 0);
	new->sd->elem[SB_ELEM_SLIDER_BG] = new_button(new, NULL, slidbg_callback, 0, 1);
	new->sd->elem[SB_ELEM_SLIDER_FG] = new_button(new, NULL, slider_callback, 0, 0);

	new->wd->flags |= WID_FLAGS_CONCEALING;

	refresh_elements(new);
	new->gen->update(new);
	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scrollbar_services services = {
	create
};


static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("Scrollbar", (void *(*)(void))create);

	script->reg_widget_attrib(widtype, "string orient",    scrollbar_get_orient,      scrollbar_set_orient,      gen_methods.update);
	script->reg_widget_attrib(widtype, "long viewsize",    scrollbar_get_view_size,   scrollbar_set_view_size,   gen_methods.update);
	script->reg_widget_attrib(widtype, "long realsize",    scrollbar_get_real_size,   scrollbar_set_real_size,   gen_methods.update);
	script->reg_widget_attrib(widtype, "long offset",      scrollbar_get_view_offset, scrollbar_set_view_offset, gen_methods.update);
	script->reg_widget_attrib(widtype, "boolean autoview", scrollbar_get_autoview,    scrollbar_set_autoview,    gen_methods.update);

	widman->build_script_lang(widtype, &gen_methods);
}


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_scrollbar(struct dope_services *d) {

	but       = d->get_module("Button 1.0");
	msg       = d->get_module("Messenger 1.0");
	script    = d->get_module("Script 1.0");
	widman    = d->get_module("WidgetManager 1.0");
	userstate = d->get_module("UserState 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_scroll_updatepos = gen_methods.updatepos;

	gen_methods.draw        = scrollbar_draw;
	gen_methods.updatepos   = scrollbar_updatepos;
	gen_methods.find        = scrollbar_find;
	gen_methods.calc_minmax = scrollbar_calc_minmax;
	gen_methods.free_data   = scrollbar_free_data;

	build_script_lang();

	d->register_module("Scrollbar 1.0", &services);
	return 1;
}
