/*
 * \brief   DOpE Window widget module
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


struct window;
#define WIDGET struct window

#include "dopestd.h"
#include "window.h"
#include "script.h"
#include "gfx.h"
#include "widget_data.h"
#include "widget_help.h"
#include "event.h"
#include "widget.h"
#include "widman.h"
#include "screen.h"
#include "winlayout.h"
#include "appman.h"
#include "userstate.h"
#include "messenger.h"
#include "keycodes.h"

static struct widman_services    *widman;
static struct gfx_services       *gfx;
static struct userstate_services *userstate;
static struct script_services    *script;
static struct winlayout_services *winlayout;
static struct appman_services    *appman;
static struct messenger_services *msg;

#define WIN_UPDATE_NEW_CONTENT  0x01
#define WIN_UPDATE_SET_STAYTOP  0x02
#define WIN_UPDATE_ELEMENTS     0x04
#define WIN_UPDATE_USERPOS      0x08

#define WIN_FLAGS_STAYTOP       0x01
#define WIN_FLAGS_BACKGROUND    0x02

struct window_data {
	s32 elements;                /* bitmask of windowelements (title, closer etc.) */
	s32 bordersize;
	s32 titlesize;
	WIDGET *elem;                /* pointer to first window element */
	WIDGET *content;
	WIDGET *kfocus;              /* current keyboard focus of this window */
	s32 min_w, min_h;
	u32 flags;
	u32 update;
	u32 bgcol;
	int ux, uy, uw, uh;          /* user defined size */
};

static GFX_CONTAINER *shadow;
static int shadow_w = 17, shadow_h = 17;
static int shadow_top, shadow_bottom;
static int shadow_left, shadow_right;

extern int config_transparency;  /* support transparent windows */
extern int config_oldresize;     /* old way of resizing windows */
extern int config_sticky;        /* sticky windows */
extern SCREEN *curr_scr;

int init_window(struct dope_services *d);


/************************
 *** HELPER FUNCTIONS ***
 ************************/

static void resize_workarea(WINDOW *w) {
	WIDGET *wa = (WIDGET *)w->wind->content;
	s32 left   = winlayout->get_left_border  (w->wind->elements);
	s32 right  = winlayout->get_right_border (w->wind->elements);
	s32 top    = winlayout->get_top_border   (w->wind->elements);
	s32 bottom = winlayout->get_bottom_border(w->wind->elements);

	if (!wa) return;

	/* add shadow */
	left   += shadow_left;
	right  += shadow_right;
	top    += shadow_top;
	bottom += shadow_bottom;

	wa->gen->set_x((WIDGET *)wa, left);
	wa->gen->set_y((WIDGET *)wa, top);
	wa->gen->set_w((WIDGET *)wa, w->wd->w - left - right);
	wa->gen->set_h((WIDGET *)wa, w->wd->h - top - bottom);
	wa->gen->updatepos((WIDGET *)wa);

	w->wd->max_w = wa->gen->get_max_w(wa) + left + right;
	w->wd->max_h = wa->gen->get_max_h(wa) + top + bottom;
	w->wd->min_w = wa->gen->get_min_w(wa) + left + right;
	w->wd->min_h = wa->gen->get_min_h(wa) + top + bottom;
}


static void adopt_win_elements(WINDOW *w, WIDGET *cw) {
	while (cw) {
		cw->gen->set_parent(cw, w);
		cw->gen->update(cw);
		cw = cw->gen->get_next(cw);
	}
}


static void destroy_win_elements(WIDGET *cw) {
	WIDGET *nw;
	while (cw) {
		nw = cw->gen->get_next(cw);
		cw->gen->set_parent(cw, NULL);
		cw->gen->dec_ref(cw);
		cw = nw;
	}
}


static inline void remove_kfocus(WINDOW *w) {
	if (w->wind->kfocus) {
		w->wind->kfocus->gen->set_kfocus(w->wind->kfocus, 0);
		w->wind->kfocus->gen->update(w->wind->kfocus);
		w->wind->kfocus->gen->dec_ref(w->wind->kfocus);
		w->wind->kfocus = NULL;
	}
}


/***************************
 *** DROP SHADOW SUPPORT ***
 ***************************/

/*** CALCULATE SQUARE ROOT ***
 *
 * The valid argument range is 0..65535.
 */
static int sqroot(u32 v) {
	u32 c = 0x8000;
	u32 res = 0;
	while (c) {
		if (v >= ((res | c)*(res | c))) res |= c;
		c = c >> 1;
	}
	return res;
}


/*** GENERATE RADIAL SHADOW IMAGE ***/
static void gen_shadow(u32 *dst, int w, int h, int mx, int my) {
	int x, y, r, ry;
	u32 *d;
	int scale = (255*255)/(w*h);

	mx += mx;
	my += my;

	ry = mx*mx + my*my;
	for (y = 0; y < h; y++) {
		d = dst;
		for (x = 0, r = ry; x < w; x++) {
			d[x] = GFX_RGBA(0, 0, 0, ((255 - MIN(sqroot(r*scale), 255))*2)/3);
			r += 4*(2*x - mx) + 4;
		}
		ry  += 4*(2*y - my) + 4;
		dst += w;
	}
}


/***********************************
 *** USERSTATE HANDLER FUNCTIONS ***
 ***********************************/

/*** VARIABLES FOR USERSTATE HANDLING ***/
static long    owx1, owy1, owx2, owy2;   /* original window area                */
static long    nwx1, nwy1, nwx2, nwy2;   /* new window area                     */
static WINDOW *curr_window;              /* currently modified window           */
static SCREEN *curr_screen;              /* screen of currently modified window */


static void win_move_motion_callback(WIDGET *cw, int dx, int dy) {
  (void)cw;
	if (config_sticky) return;
	if (!curr_window || !curr_screen) return;
	nwx1 = owx1 + dx;
	nwy1 = owy1 + dy;
	curr_screen->scr->place(curr_screen, curr_window, nwx1, nwy1, NOARG, NOARG);
	if (dx || dy) {
		unsigned long m = curr_window->gen->get_bind_msg((WIDGET *)curr_window, "move");
		s32  id = curr_window->gen->get_app_id((WIDGET *)curr_window);
		if (m) msg->send_action_event(id, "move", m);
	}
}


static void win_move_leave_callback(WIDGET *cw, int dx, int dy) {
	if (!curr_window) return;

	if (dx || dy) {
		unsigned long m  = curr_window->gen->get_bind_msg((WIDGET *)curr_window, "moved");
		s32  id = curr_window->gen->get_app_id((WIDGET *)curr_window);
		if (m) msg->send_action_event(id, "moved", m);
	}

	if (cw) {
		cw->gen->set_state(cw, 0);
		cw->gen->update(cw);
	}
}


static void win_resize_motion_callback(WIDGET *cw, int dx, int dy) {
	long size_flags = 0;
	long min_w, max_w, min_h, max_h;

	if (config_sticky) return;
	if (!cw || !curr_window || !curr_screen) return;

	size_flags = (long)cw->gen->get_context(cw);
	min_w = curr_window->gen->get_min_w((WIDGET *)curr_window);
	max_w = curr_window->gen->get_max_w((WIDGET *)curr_window);
	min_h = curr_window->gen->get_min_h((WIDGET *)curr_window);
	max_h = curr_window->gen->get_max_h((WIDGET *)curr_window);

	/* flag for left resizing */
	if (size_flags & 1) {
		nwx1 = owx1 + dx;
		if (!config_oldresize) {
			if (nwx1 + min_w - 1 > owx2)      nwx2 = nwx1 + min_w - 1;
			else if (nwx1 + max_w - 1 < owx2) nwx2 = nwx1 + max_w - 1;
			else nwx2 = owx2;
		} else {
			if (nwx1 + min_w - 1 > owx2) nwx1 = owx2 - min_w + 1;
			if (nwx1 + max_w - 1 < owx2) nwx1 = owx2 - max_w + 1;
		}
	}

	/* flag for right resizing */
	if (size_flags & 4) {
		nwx2 = owx2 + dx;
		if (!config_oldresize) {
			if (owx1 + min_w - 1 > nwx2)      nwx1 = nwx2 - min_w + 1;
			else if (owx1 + max_w - 1 < nwx2) nwx1 = nwx2 - max_w + 1;
			else nwx1 = owx1;
		} else {
			if (owx1 + min_w - 1 > nwx2) nwx2 = owx1 + min_w - 1;
			if (owx1 + max_w - 1 < nwx2) nwx2 = owx1 + max_w - 1;
		}
	}

	/* flag for top resizing */
	if (size_flags & 2) {
		nwy1 = owy1 + dy;
		if (!config_oldresize) {
			if (nwy1 + min_h - 1 > owy2) nwy2 = nwy1 + min_h - 1;
			else if (nwy1 + max_h - 1 < owy2) nwy2 = nwy1 + max_h - 1;
			else nwy2 = owy2;
		} else {
			if (nwy1 + min_h - 1 > owy2) nwy1 = owy2 - min_h + 1;
			if (nwy1 + max_h - 1 < owy2) nwy1 = owy2 - max_h + 1;
		}
	}

	/* flag for bottom resizing */
	if (size_flags & 8) {
		nwy2 = owy2 + dy;
		if (!config_oldresize) {
			if (owy1 + min_h - 1 > nwy2) nwy1 = nwy2 - min_h + 1;
			else if (owy1 + max_h - 1 <nwy2) nwy1 = nwy2 - max_h + 1;
			else nwy1 = owy1;
		} else {
			if (owy1 + min_h - 1 > nwy2) nwy2 = owy1 + min_h - 1;
			if (owy1 + max_h - 1 < nwy2) nwy2 = owy1 + max_h - 1;
		}
	}

	curr_screen->scr->place(curr_screen, curr_window, nwx1, nwy1, nwx2-nwx1+1, nwy2-nwy1+1);
}


static void win_resize_leave_callback(WIDGET *cw, int dx, int dy) {
	if (!curr_window) return;

	if (dx || dy) {
		unsigned long m = curr_window->gen->get_bind_msg((WIDGET *)curr_window, "resized");
		s32  id = curr_window->gen->get_app_id((WIDGET *)curr_window);
		if (m) msg->send_action_event(id, "resized", m);
		if ((owx1 != nwx1) || (owy1 != nwy1)) {
			m = curr_window->gen->get_bind_msg((WIDGET *)curr_window, "moved");
			if (m) msg->send_action_event(id, "moved", m);
		}
	}

	if ((dx>-2) && (dx<2) && (dy>-2) && (dy<2)) {
		curr_window->win->top(curr_window);
	}

	if (cw) {
		cw->gen->set_state(cw, 0);
		cw->gen->update(cw);
	}
}


/*** GET X POSITITION OF THE WINDOW'S WORK AREA ***/
static int win_get_workx(WINDOW *w) {
	return w->wd->x + winlayout->get_left_border(w->wind->elements) + shadow_left;
}


/*** GET Y POSITITION OF THE WINDOW'S WORK AREA ***/
static int win_get_worky(WINDOW *w) {
	return w->wd->y + winlayout->get_top_border(w->wind->elements) + shadow_top;
}


/*** GET WIDTH OF THE WINDOW'S WORK AREA ***/
static int win_get_workw(WINDOW *w) {
	return w->wd->w - winlayout->get_left_border(w->wind->elements)
	                - winlayout->get_right_border(w->wind->elements)
	                - shadow_left - shadow_right;
}


/*** GET HEIGHT OF THE WINDOW'S WORK AREA ***/
static int win_get_workh(WINDOW *w) {
	return w->wd->h - winlayout->get_top_border(w->wind->elements)
	                - winlayout->get_bottom_border(w->wind->elements)
	                - shadow_top - shadow_bottom;
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static void (*orig_set_app_id) (WIDGET *w, s32 app_id);
static void win_set_app_id(WINDOW *w, s32 app_id) {

	SCREEN *scr = (SCREEN *)w->gen->get_parent(w);
	char *app_name = appman->get_app_name(app_id);

	if (app_name) {
		winlayout->set_win_title(w->wind->elem, app_name);

		/* register window title at screen */
		if (scr) scr->scr->set_title(scr, w, app_name);;
	}

	if (orig_set_app_id) orig_set_app_id((WIDGET *)w, app_id);
}


/*** DRAW WINDOW BACKGROUND ***/
static int win_draw_bg(WINDOW *cw, struct gfx_ds *ds, long x, long y, long w, long h,
                       WIDGET *origin, int opaque) {
	int ret = 0;

	if (origin == cw) return 1;
	if ((w <= 0) || (h <= 0)) return 0;

	if (config_transparency) {
		u32 bgcol = (cw->wind->bgcol & 0xffffff00) | (opaque ? 0xff : 0x7f);
		gfx->push_clipping(ds, x, y, w, h);

		if (!opaque)
			ret |= cw->gen->drawbehind(cw, cw, 0, 0, cw->wd->w, cw->wd->h, origin);

		/*
		 * If the drawbehind function performed any graphics operations,
		 * we need to paint the foreground, too.
		 */
		if ((ret || opaque) && !origin)
			gfx->draw_box(ds, x, y, w, h, bgcol);

		gfx->pop_clipping(ds);
	} else {
		if (!origin) {
			gfx->draw_box(ds, x, y, w, h, 0x708090ff);
			ret |= 1;
		}
	}
	return ret;
}


static void draw_shadow(struct gfx_ds *ds, int x, int y, int w, int h) {
	int sw2 = shadow_w >> 1, sh2 = shadow_h >> 1;

	/* top left */
	gfx->draw_slice(ds, x, y, sw2, shadow_top, 0, 0, sw2, shadow_top, shadow, 255);
	gfx->draw_slice(ds, x, y + shadow_top, shadow_left, sh2 - shadow_top,
	                    0, 0, shadow_left, sh2 - shadow_top, shadow, 255);

	/* top */
	gfx->draw_slice(ds, x + sw2, y, w - sw2*2, shadow_top,
	                    sw2, 0, 1, shadow_top, shadow, 255);
	/* top right */
	gfx->draw_slice(ds, x + w - sw2, y, sw2, shadow_top,
	                    sw2 + 1, 0, sw2, shadow_top, shadow, 255);
	gfx->draw_slice(ds, x + w - shadow_right, y + shadow_top, shadow_right, sh2 - shadow_top,
	                    shadow_w - shadow_right - 1, 0, shadow_right, sh2 - shadow_top, shadow, 255);

	/* left */
	gfx->draw_slice(ds, x, y + sh2, shadow_left, h - sh2*2,
	                    0, sh2, shadow_left, 1, shadow, 255);
	/* right */
	gfx->draw_slice(ds, x + w - shadow_right, y + sh2, shadow_right, h - sh2*2,
	                    shadow_w - shadow_right - 1, sh2, shadow_right, 1, shadow, 255);
	/* bottom left */
	gfx->draw_slice(ds, x, y + h - sh2, sw2, sh2,
	                    0, sh2 + 1, sw2, sh2, shadow, 255);
	/* bottom */
	gfx->draw_slice(ds, x + sw2, y + h - sh2, w - sw2*2, sh2,
	                    sw2 + 1, sh2 + 1, 1, sh2, shadow, 255);
	/* bottom right */
	gfx->draw_slice(ds, x + w - sw2, y + h - sh2, sw2, sh2,
	                    sw2 + 1, sh2 + 1, sw2, sh2, shadow, 255);
}


extern int transparency_depth;  /* from screen.c */

static int win_draw(WINDOW *w, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	int x1, y1, x2, y2;
	int cx1 = gfx->get_clip_x(ds);
	int cy1 = gfx->get_clip_y(ds);
	int cx2 = gfx->get_clip_w(ds) + cx1 - 1;
	int cy2 = gfx->get_clip_h(ds) + cy1 - 1;
	int ret = 0;
	WIDGET *cw;

	/* we hit the origin, acknowledge visibility request */
	if (origin == w) return 1;

	/* draw window content */
	cw = w->wind->content;

	/* for windows with no or non-concealing content we need to draw a background */
	if (!cw || (cw && !(cw->wd->flags & WID_FLAGS_CONCEALING))) {
		ret |= w->gen->draw_bg(w, ds, win_get_workx(w), win_get_worky(w),
		                              win_get_workw(w), win_get_workh(w),
		                              origin, 0);
	}

	if ((cx1 < w->wd->x + x + shadow_left) || (cx2 > w->wd->x + x + w->wd->w - 1 - shadow_right)
	 || (cy1 < w->wd->y + y + shadow_top)  || (cy2 > w->wd->y + y + w->wd->h - 1 - shadow_bottom)) {
		int sret = 0; 

		/*
		 * For drawing the background of the shadow,
		 * we turn off the transparency depth limiting
		 * by decreasing the current depth value
		 * temporary.
		 */

		/* draw shadow background */
		transparency_depth--;
		sret |= w->gen->drawbehind(w, w, 0, 0, w->wd->w, shadow_top, origin);
		sret |= w->gen->drawbehind(w, w, 0, shadow_top, shadow_left, w->wd->h - shadow_top - shadow_bottom, origin);
		sret |= w->gen->drawbehind(w, w, w->wd->w - shadow_right, shadow_top, shadow_left, w->wd->h - shadow_top - shadow_bottom, origin);
		sret |= w->gen->drawbehind(w, w, 0, w->wd->h - shadow_bottom, w->wd->w, shadow_bottom, origin);
		transparency_depth++;

		if (sret) draw_shadow(ds, w->wd->x + x, w->wd->y + y, w->wd->w, w->wd->h);
		ret |= sret;
	}

	if (cw) {
		x1 = cw->gen->get_x(cw) + w->wd->x + x;
		y1 = cw->gen->get_y(cw) + w->wd->y + y;
		x2 = x1 + cw->gen->get_w(cw) - 1;
		y2 = y1 + cw->gen->get_h(cw) - 1;
		gfx->push_clipping(ds, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
		ret |= cw->gen->draw(cw, ds, w->wd->x + x, w->wd->y + y, origin);
		gfx->pop_clipping(ds);
	}

	/* draw window elements */
	cw = w->wind->elem;
	while (cw) {
		x1 = cw->gen->get_x(cw) + w->wd->x + x;
		y1 = cw->gen->get_y(cw) + w->wd->y + y;
		x2 = x1 + cw->gen->get_w(cw) - 1;
		y2 = y1 + cw->gen->get_h(cw) - 1;
		gfx->push_clipping(ds, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
		ret |= cw->gen->draw(cw, ds, w->wd->x + x, w->wd->y + y, origin);
		gfx->pop_clipping(ds);
		cw = cw->gen->get_next(cw);
	}

	return ret;
}


static void win_calc_minmax(WINDOW *w) {
	WIDGET *wa = (WIDGET *)w->wind->content;
	int left   = winlayout->get_left_border  (w->wind->elements);
	int right  = winlayout->get_right_border (w->wind->elements);
	int top    = winlayout->get_top_border   (w->wind->elements);
	int bottom = winlayout->get_bottom_border(w->wind->elements);

	if (wa) {
		w->wd->max_w = wa->gen->get_max_w(wa);
		w->wd->max_h = wa->gen->get_max_h(wa);
		w->wd->min_w = wa->gen->get_min_w(wa);
		w->wd->min_h = wa->gen->get_min_h(wa);
	} else {
		w->wd->min_w = w->wd->min_h = 0;
		w->wd->max_w = w->wd->max_h = 9999;
	}

	w->wd->max_w += left + right  + shadow_left + shadow_right;
	w->wd->max_h += top  + bottom + shadow_top  + shadow_bottom;
	w->wd->min_w += left + right  + shadow_left + shadow_right;
	w->wd->min_h += top  + bottom + shadow_top  + shadow_bottom;
}


static void (*orig_update) (WINDOW *w);
static void win_update(WINDOW *w) {

	/*
	 * For windows there exist the attributes x, y, w, h and workx, worky, workw and
	 * workh. Normally, widgets can not set their position and size attributes but
	 * windows play a special role. Such attribute changes are propagated to the
	 * screen that holds the window.
	 */
	if (w->wind->update & WIN_UPDATE_USERPOS) {
		SCREEN *parent = (SCREEN *)w->gen->get_parent(w);

		/*
		 * If the window is managed by a screen, tell the screen to place the
		 * window. Otherwise, we just set the window's position attributes.
		 */
		if (parent && parent->gen->is_root((WIDGET *)parent)) {
			parent->scr->place(parent, w, w->wind->ux, w->wind->uy, w->wind->uw, w->wind->uh);
			w->wind->ux = w->wind->uy = w->wind->uw = w->wind->uh = NOARG;
		} else {
			if (w->wind->ux != NOARG) w->wd->x = w->wind->ux;
			if (w->wind->uy != NOARG) w->wd->y = w->wind->uy;
			if (w->wind->uw != NOARG) w->wd->w = w->wind->uw;
			if (w->wind->uh != NOARG) w->wd->h = w->wind->uh;
		}
	}
	
	orig_update(w);
}


static void (*orig_updatepos) (WINDOW *w);
static void win_updatepos(WINDOW *w) {
	winlayout->resize_win_elements(w->wind->elem, w->wind->elements,
	                               shadow_left, shadow_top,
	                               w->wd->w - shadow_left - shadow_right,
	                               w->wd->h - shadow_top - shadow_bottom);
	resize_workarea(w);
	orig_updatepos(w);
}


static WIDGET *win_find(WINDOW *w, long x, long y) {
	WIDGET *result;
	WIDGET *c;

	/* check if position is inside the window */
	if ((x >= w->wd->x + shadow_left)
	 && (y >= w->wd->y + shadow_top)
	 && (x < w->wd->x + w->wd->w - shadow_right)
	 && (y < w->wd->y + w->wd->h - shadow_bottom)) {

		/* we are hit - lets check our children */
		c = (WIDGET *)w->wind->content;
		if (c) {
			result = c->gen->find(c, x-w->wd->x, y-w->wd->y);
			if (result) return result;
		}

		/* window elements */
		c = w->wind->elem;
		while (c) {
			result = c->gen->find(c, x-w->wd->x, y-w->wd->y);
			if (result) return result;
			c = c->gen->get_next(c);
		}
		return w;
	}
	return NULL;
}


static void win_handle_event(WIDGET *w, EVENT *ev, WIDGET *from) {
	struct binding *cb;

	/*
	 * If we get an event for changing the keyboard focus,
	 * the keyboard focus went already through all children
	 * and we need to wrap it.
	 */
	if (from && (ev->type == L4RE_EV_KEY && ev->value == 1) && w->wind->content
	 && (ev->code == L4RE_KEY_TAB)) {
		WIDGET *nw = w->wind->content->gen->first_kfocus(w->wind->content);
		if (nw) nw->gen->focus(nw);
//		return;
	}

	/* check bindings */
	for (cb = w->wd->bindings; cb; cb = cb->next)
		if (cb->ev_type == ev->type)
			msg->send_input_event(w->wd->app_id, ev, cb->msg);

	if ((ev->type != L4RE_EV_KEY || ev->value != 1) || (ev->code != L4RE_BTN_LEFT)) return;

	curr_window = w;
	if (!curr_window) return;
	
	nwx1 = owx1 = curr_window->gen->get_x((WIDGET *)w);
	nwy1 = owy1 = curr_window->gen->get_y((WIDGET *)w);
	
	curr_screen = (SCREEN *)w->gen->get_parent(w);
	if (!curr_screen) return;
	
	userstate->drag(w, win_move_motion_callback, NULL, win_move_leave_callback);
}


/*** FREE REFERENCED DATA ***/
static void win_free_data(WINDOW *w) {
	remove_kfocus(w);
	destroy_win_elements(w->wind->elem);
	destroy_win_elements(w->wind->content);
}


static char *win_get_type(WINDOW *w) {
  (void)w;
	return "Window";
}


/*** REMOVE CHILD WIDGET FROM WINDOW ***/
static void win_remove_child(WINDOW *w, WIDGET *child) {

	/* sanity checks */
	if ((w->wind->content != child) || (child->gen->get_parent(child) != w)) return;

	/* release child */
	w->wind->content = NULL;
	child->gen->set_parent(child, NULL);
	child->gen->dec_ref(child);

	/* update the childless window */
	w->wd->update |= WID_UPDATE_MINMAX;
	w->gen->update(w);
}


/*******************************
 *** WINDOW SPECIFIC METHODS ***
 *******************************/

/*** SET NEW WINDOW CONTENT ***/
static void win_set_content(WINDOW *w, WIDGET *new_content) {

	if (!new_content) return;

	/* avoid cyclic parent relationships */
	if (new_content->gen->related_to(new_content, w)) return;

	/* kindly release old child */
	if (w->wind->content) {
		w->wind->content->gen->set_parent(w->wind->content, NULL);
		w->wind->content->gen->dec_ref(w->wind->content);
	}

	new_content->gen->inc_ref(new_content);

	/* release from old parent */
	new_content->gen->release(new_content);

	/* adopt new child */
	w->wind->content = new_content;
	new_content->gen->set_parent(new_content, w);

	w->wd->update |= WID_UPDATE_MINMAX;
}


static WIDGET *win_get_content(WINDOW *w) {
	return w->wind->content;
}


static void win_open(WINDOW *w) {
	SCREEN *scr = curr_scr;
	if (!scr) return;

	/* use user-specified window position and size */
	scr->scr->place(scr, w, w->wind->ux, w->wind->uy, w->wind->uw, w->wind->uh);

	/* reset user-specified window position and size */
	w->wind->ux = w->wind->uy = w->wind->uw = w->wind->uh = NOARG;
}


static void win_close(WINDOW *w) {
	SCREEN *scr = (SCREEN *)w->gen->get_parent(w);
	if (!scr) return;
	scr->scr->remove(scr, w);
}


static void win_top(WINDOW *w) {
	SCREEN *scr;
	unsigned long message;

	scr = (SCREEN *)w->gen->get_parent(w);
	if (scr) scr->scr->top(scr, w);;

	message = w->gen->get_bind_msg(w, "top");
	if (message) msg->send_action_event(w->wd->app_id, "top", message);

	userstate->set_active_window(w, 0);
}


static void win_back(WINDOW *w) {
	SCREEN *scr = (SCREEN *)w->gen->get_parent(w);
	if (scr) scr->scr->back(scr, w);;
}


static void win_activate(WINDOW *w) {
	SCREEN *scr;
	scr = (SCREEN *)w->gen->get_parent(w);
	if (!scr) return;
	scr->scr->set_act_win(scr, w);
	if (!w->wind->kfocus && w->wind->content) {
		WIDGET *first_kfocus = w->wind->content->gen->first_kfocus(w->wind->content);
		if (first_kfocus) first_kfocus->gen->focus(first_kfocus);
	}
}


static void win_set_staytop(WINDOW *w, s16 staytop_flag) {
	if (staytop_flag) {
		w->wind->flags = w->wind->flags | WIN_FLAGS_STAYTOP;
	} else {
		w->wind->flags = w->wind->flags & ~WIN_FLAGS_STAYTOP;
	}
	w->wind->update = w->wind->update | WIN_UPDATE_SET_STAYTOP;
}


static s16 win_get_staytop(WINDOW *w) {
	if (w->wind->flags & WIN_FLAGS_STAYTOP) return 1;
	return 0;
}


static void win_set_elem_mask(WINDOW *w, s32 new_elem_mask) {
	w->wind->elements = new_elem_mask;
	destroy_win_elements(w->wind->elem);
	w->wind->elem = winlayout->create_win_elements(w->wind->elements,
	                                               shadow_left, shadow_top,
	                                               w->wd->w - shadow_left - shadow_right,
	                                               w->wd->h - shadow_top - shadow_bottom);
	adopt_win_elements(w, w->wind->elem);
	w->wd->update = w->wd->update | WID_UPDATE_MINMAX;
}


static s32 win_get_elem_mask(WINDOW *w) {
	return w->wind->elements;
}


static void win_set_state(WINDOW *w, s32 state) {
	winlayout->set_win_state(w->wind->elem, state);
}


static void win_set_title(WINDOW *w, char *new_title) {
	SCREEN *scr = (SCREEN *)w->gen->get_parent(w);

	if (!new_title) return;
	winlayout->set_win_title(w->wind->elem, new_title);

	/* register window title at screen */
	if (scr) scr->scr->set_title(scr, w, new_title);;
}


static char *win_get_title(WINDOW *w) {
	return winlayout->get_win_title(w->wind->elem);
}


static void win_set_bgcolor(WINDOW *w, char *new_bgcolor) {
	if (!new_bgcolor) return;
	if (dope_streq(new_bgcolor, "black", 255))
		w->wind->bgcol = 0x000000ff;
}


static void win_set_background(WINDOW *w, s16 bg_flag) {
	if (bg_flag) {
		w->wind->flags = w->wind->flags | WIN_FLAGS_BACKGROUND;
	} else {
		w->wind->flags = w->wind->flags & ~WIN_FLAGS_BACKGROUND;
	}
	w->wind->update |= WIN_UPDATE_NEW_CONTENT;
}


static s16 win_get_background(WINDOW *w) {
	if (w->wind->flags & WIN_FLAGS_BACKGROUND) return 1;
	return 0;
}


/*** SET/GET X POSITITION OF THE WINDOW'S OUTER AREA ***/
static void win_set_x(WINDOW *w, int new_x) {
	w->wind->ux = new_x;
	w->wind->update |= WIN_UPDATE_USERPOS;
}
static int win_get_x(WINDOW *w) {
	return w->wd->x;
}


/*** SET/GET Y POSITITION OF THE WINDOW'S OUTER AREA ***/
static void win_set_y(WINDOW *w, int new_y) {
	w->wind->uy = new_y;
	w->wind->update |= WIN_UPDATE_USERPOS;
}
static int win_get_y(WINDOW *w) {
	return w->wd->y;
}


/*** SET/GET WIDTH OF THE WINDOW'S OUTER AREA ***/
static void win_set_w(WINDOW *w, int new_w) {
	w->wind->uw = new_w;
	w->wind->update |= WIN_UPDATE_USERPOS;
}
static int win_get_w(WINDOW *w) {
	return w->wd->w;
}


/*** SET/GET HEIGHT OF THE WINDOW'S OUTER AREA ***/
static void win_set_h(WINDOW *w, int new_h) {
	w->wind->uh = new_h;
	w->wind->update |= WIN_UPDATE_USERPOS;
}
static int win_get_h(WINDOW *w) {
	return w->wd->h;
}


static void win_handle_resize(WINDOW *w, WIDGET *cw) {
  (void)w;

	if (!cw) return;

	cw->gen->set_state(cw, 1);
	cw->gen->update(cw);

	/* determine the associated window and its current position */
	curr_window = (WINDOW *)cw->gen->get_window(cw);
	if (!curr_window) return;
	
	nwx1 = owx1 = curr_window->gen->get_x((WIDGET *)curr_window);
	nwy1 = owy1 = curr_window->gen->get_y((WIDGET *)curr_window);
	nwx2 = owx2 = owx1 + curr_window->gen->get_w((WIDGET *)curr_window) - 1;
	nwy2 = owy2 = owy1 + curr_window->gen->get_h((WIDGET *)curr_window) - 1;

	curr_screen = (SCREEN *)curr_window->gen->get_parent(curr_window);
	if (!curr_screen) return;

	userstate->drag(cw, win_resize_motion_callback, NULL, win_resize_leave_callback);
}


static void win_handle_move(WINDOW *w, WIDGET *cw) {
  (void)w;

	if (!cw) return;

	cw->gen->set_state(cw, 1);
	cw->gen->update(cw);

	/* determine the associated window and its current position */
	curr_window = (WINDOW *)cw->gen->get_window(cw);
	if (!curr_window) return;

	nwx1 = owx1 = curr_window->gen->get_x((WIDGET *)curr_window);
	nwy1 = owy1 = curr_window->gen->get_y((WIDGET *)curr_window);
	
	curr_screen = (SCREEN *)curr_window->gen->get_parent(curr_window);
	if (!curr_screen) return;
	
	curr_window->win->top(curr_window);

	userstate->drag(cw, win_move_motion_callback, NULL, win_move_leave_callback);
}


/*** SET X POSITITION OF THE WINDOW'S WORK AREA ***/
static void win_set_workx(WINDOW *w, int new_workx) {
	win_set_x(w, new_workx - winlayout->get_left_border(w->wind->elements) - shadow_left);
}


/*** SET Y POSITITION OF THE WINDOW'S WORK AREA ***/
static void win_set_worky(WINDOW *w, int new_worky) {
	win_set_y(w, new_worky - winlayout->get_top_border(w->wind->elements) - shadow_top);
}


/*** SET WIDTH OF THE WINDOW'S WORK AREA ***/
static void win_set_workw(WINDOW *w, int new_workw) {
	win_set_w(w, new_workw + winlayout->get_left_border(w->wind->elements)
	                       + winlayout->get_right_border(w->wind->elements)
	                       + shadow_left + shadow_right);
}


/*** SET HEIGHT OF THE WINDOW'S WORK AREA ***/
static void win_set_workh(WINDOW *w, int new_workh) {
	win_set_h(w, new_workh + winlayout->get_top_border(w->wind->elements)
	                       + winlayout->get_bottom_border(w->wind->elements)
	                       + shadow_top + shadow_bottom);
}


/*** SET CURRENT KEYBOARD FOCUS OF A WINDOW ***/
static void win_set_kfocus(WINDOW *w, WIDGETARG *kfocus) {

	/* only accept widet that can actually take the focus */
	if (!kfocus || !(kfocus->wd->flags & WID_FLAGS_TAKEFOCUS)) return;

	/* get rid of the old keyboard focus */
	remove_kfocus(w);

	/* set new keyboard focus */
	w->wind->kfocus = kfocus;

	/* update new keyboard focus */
	if (w->wind->kfocus) {
		w->wind->kfocus->gen->inc_ref(w->wind->kfocus);
		w->wind->kfocus->gen->set_kfocus(w->wind->kfocus, 1);
		w->wind->kfocus->gen->update(w->wind->kfocus);
	}
}


/*** REQUEST CURRENT KEYBOARD FOCUS OF A WINDOW ***/
static WIDGET *win_get_kfocus(WINDOW *w) {
	return w->wind->kfocus;
}


static struct widget_methods gen_methods;
static struct window_methods win_methods = {
	win_set_content,
	win_get_content,
	win_set_staytop,
	win_get_staytop,
	win_set_elem_mask,
	win_get_elem_mask,
	win_set_title,
	win_get_title,
	win_set_background,
	win_get_background,
	win_set_state,
	win_activate,
	win_open,
	win_close,
	win_top,
	win_handle_move,
	win_handle_resize,
	win_set_x,
	win_set_y,
	win_set_kfocus,
	win_get_kfocus,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static WINDOW *create(void) {

	WINDOW *new = ALLOC_WIDGET(struct window);
	SET_WIDGET_DEFAULTS(new, struct window, &win_methods);

	new->wd->x = 50;
	new->wd->y = 50;
	new->wd->w = 128;
	new->wd->h = 128;
	new->wd->max_w = 3000;
	new->wd->max_h = 3000;

	/* set window specific attributes */
	new->wind->flags    =  WIN_FLAGS_BACKGROUND;
	new->wind->elements =  WIN_CLOSER  | WIN_FULLER | WIN_TITLE | WIN_BORDERS;
	new->wind->ux = new->wind->uy = new->wind->uw = new->wind->uh = NOARG;
	new->wind->bgcol = 0x708090ff;

	/* create window elements */
	new->wind->elem = winlayout->create_win_elements(new->wind->elements,
	                                                 shadow_left, shadow_top,
	                                                 new->wd->w - shadow_left - shadow_right,
	                                                 new->wd->h - shadow_top - shadow_bottom);
	/* tell all window elements about its proud parent */
	adopt_win_elements(new, new->wind->elem);

	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct window_services services = {
	create,
	5, 5, 1, 8  /* drop shadow configuration */
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype = script->reg_widget_type("Window", (void *(*)(void))create);

	script->reg_widget_method(widtype, "void open(void)",  &win_open);
	script->reg_widget_method(widtype, "void close(void)", &win_close);
	script->reg_widget_method(widtype, "void top(void)",   &win_top);
	script->reg_widget_method(widtype, "void back(void)",  &win_back);

	script->reg_widget_attrib(widtype, "Widget content", win_get_content, win_set_content, win_update);
	script->reg_widget_attrib(widtype, "boolean staytop", win_get_staytop, win_set_staytop, win_update);
	script->reg_widget_attrib(widtype, "boolean background", win_get_background, win_set_background, win_update);
	script->reg_widget_attrib(widtype, "string title", win_get_title, win_set_title, NULL);
	script->reg_widget_attrib(widtype, "string bgcolor", NULL, win_set_bgcolor, NULL);
	script->reg_widget_attrib(widtype, "long x", win_get_x, win_set_x, win_update);
	script->reg_widget_attrib(widtype, "long y", win_get_y, win_set_y, win_update);
	script->reg_widget_attrib(widtype, "long w", win_get_w, win_set_w, win_update);
	script->reg_widget_attrib(widtype, "long h", win_get_h, win_set_h, win_update);
	script->reg_widget_attrib(widtype, "long workx", win_get_workx, win_set_workx, win_update);
	script->reg_widget_attrib(widtype, "long worky", win_get_worky, win_set_worky, win_update);
	script->reg_widget_attrib(widtype, "long workw", win_get_workw, win_set_workw, win_update);
	script->reg_widget_attrib(widtype, "long workh", win_get_workh, win_set_workh, win_update);

	widman->build_script_lang(widtype, &gen_methods);
}


int init_window(struct dope_services *d) {

	gfx       = d->get_module("Gfx 1.0");
	widman    = d->get_module("WidgetManager 1.0");
	userstate = d->get_module("UserState 1.0");
	script    = d->get_module("Script 1.0");
	winlayout = d->get_module("WinLayout 1.0");
	appman    = d->get_module("ApplicationManager 1.0");
	msg       = d->get_module("Messenger 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);
	orig_updatepos           = gen_methods.updatepos;
	orig_update              = gen_methods.update;
	orig_set_app_id          = gen_methods.set_app_id;

	gen_methods.get_type     = &win_get_type;
	gen_methods.draw         = &win_draw;
	gen_methods.update       = &win_update;
	gen_methods.updatepos    = &win_updatepos;
	gen_methods.set_app_id   = &win_set_app_id;
	gen_methods.find         = &win_find;
	gen_methods.handle_event = &win_handle_event;
	gen_methods.calc_minmax  = &win_calc_minmax;
	gen_methods.free_data    = &win_free_data;
	gen_methods.remove_child = &win_remove_child;
	gen_methods.draw_bg      = &win_draw_bg;

	/* register script commands */
	build_script_lang();

	/* init drop shadow */
	shadow = gfx->alloc_img(shadow_w, shadow_h, GFX_IMG_TYPE_RGBA32);
	gen_shadow(gfx->map(shadow), shadow_w, shadow_h, shadow_w >> 1, shadow_h >> 1);

	shadow_left   = services.shadow_left;
	shadow_right  = services.shadow_right;
	shadow_top    = services.shadow_top;
	shadow_bottom = services.shadow_bottom;

	d->register_module("Window 1.0", &services);
	return 1;
}
