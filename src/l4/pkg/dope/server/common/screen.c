/*
 * \brief   DOpE Screen widget module
 * \date    2003-11-16
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * NOTES:
 * - Get rid of window specific stuff. Maybe by making the staytop-bit
 *   a generic widget function. Finally, a screen should be able to
 *   hold any kind of widget.
 * - Draw function could be implemented. Thus, screens can be stacked.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

struct screen;
#define WIDGET struct screen

#include "dopestd.h"
#include "widget_data.h"
#include "widget_help.h"
#include "screen.h"
#include "script.h"
#include "widman.h"
#include "appman.h" /* FIXME: temporary !!! */
#include "viewman.h"
#include "window.h"
#include "container.h"
#include "background.h"
#include "button.h"
#include "frame.h"
#include "redraw.h"
#include "userstate.h"
#include "gfx.h"

static struct userstate_services  *userstate;
static struct background_services *bg;
static struct widman_services     *widman;
static struct script_services     *script;
static struct appman_services     *appman;
static struct redraw_services     *redraw;
static struct viewman_services    *viewman;
static struct button_services     *but;
static struct container_services  *cont;
static struct window_services     *win;
static struct gfx_services        *gfx;
static struct frame_services      *frame;

struct screen_data {
	WIDGET *first_win;       /* first window of window stack               */
	WIDGET *active_win;      /* window that holds the keyboard focus       */
	struct gfx_ds *scr_ds;   /* GFX container to use for the screen output */
	BUTTON *menubutton;      /* Button displaying the name of active win   */
	SCREEN *next;            /* next screen in the screen list             */
};

int init_screen(struct dope_services *d);

SCREEN *first_scr;
SCREEN *curr_scr;

extern int config_menubar;


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

static int draw_rec(GFX_CONTAINER *ds, WIDGET *cw, WIDGET *origin,
                    long cx1, long cy1, long cx2, long cy2, int do_update) {
	long   sx1, sy1, sx2, sy2;
	static long d;
	int need_update = 0;
	WIDGET *next;
	if (!cw) return 0;

	/* calc intersection between dirty area and current window */
	sx1 = MAX(cx1, (d = cw->gen->get_x(cw)));
	sx2 = MIN(cx2,  d + cw->gen->get_w(cw) - 1);
	sy1 = MAX(cy1, (d = cw->gen->get_y(cw)));
	sy2 = MIN(cy2,  d + cw->gen->get_h(cw) - 1);

	/* if there is an intersection - subdivide area */
	if ((sx1 <= sx2) && (sy1 <= sy2)) {

		gfx->push_clipping(ds, sx1, sy1, sx2 - sx1 + 1, sy2 - sy1 + 1);

		/*
		 * If an origin is specified, first find out if the specified
		 * origin is visible at the current screen area and then, draw
		 * it (by specifying NULL as origin). In fact, if we specify
		 * an origin != NULL, no drawing is performed at all.
		 */
		need_update |= cw->gen->draw(cw, ds, 0, 0, origin);
		if (origin && need_update)
			cw->gen->draw(cw, ds, 0, 0, NULL);

		gfx->pop_clipping(ds);
		if (need_update && do_update)
			gfx->update(ds, sx1, sy1, sx2 - sx1 + 1, sy2 - sy1 + 1);

		/* take care about the rest */
		if ((next = cw->gen->get_next(cw)) == NULL) return need_update;
		if (sx1 > cx1) need_update |= draw_rec(ds, next, origin, cx1, MAX(cy1, sy1), sx1 - 1, MIN(cy2, sy2), do_update);
		if (sy1 > cy1) need_update |= draw_rec(ds, next, origin, cx1, cy1, cx2, sy1 - 1, do_update);
		if (sx2 < cx2) need_update |= draw_rec(ds, next, origin, sx2 + 1, MAX(cy1, sy1), cx2, MIN(cy2, sy2), do_update);
		if (sy2 < cy2) need_update |= draw_rec(ds, next, origin, cx1, sy2 + 1, cx2, cy2, do_update);
	} else {
		need_update |= draw_rec(ds, cw->gen->get_next(cw), origin, cx1, cy1, cx2, cy2, do_update);
	}
	return need_update;
}


/*** DETERMINE THE LAST 'STAYTOP'-WINDOW OF THE WINDOW STACK ***/
static WIDGET *get_last_staytop_win(SCREEN *scr) {
	WINDOW *ltw = (WINDOW *)scr->sd->first_win, *cw;
	while (ltw) {
		cw = (WINDOW *)ltw->wd->next;
		if (!cw) break;
		if (!cw->win->get_staytop(cw)) break;
		if (!ltw->win->get_staytop(ltw)) break;
		ltw = cw;
	}
	if (ltw) {
		if (!ltw->win->get_staytop(ltw)) ltw = NULL;
	}
	return (WIDGET *)ltw;
}


///*** DUMP WINDOW STACK INFORMATION ***/
//static void scr_print_info(SCREEN *scr) {
//	WIDGET *cw;
//
//	for (cw = scr->sd->first_win; cw; cw = cw->wd->next) {
//		printf("window %p (xywh): %d, %d, %d, %d", cw,
//		       (int)cw->gen->get_x(cw), (int)cw->gen->get_y(cw),
//		       (int)cw->gen->get_w(cw), (int)cw->gen->get_h(cw));
//		if (cw->wd->parent != scr) printf("parent is not the screen!\n");
//		printf("\n");
//	}
//}


/*** CALLBACK THAT IS CALLED WHEN USER CLICKS AT THE MENUBAR ***/
static void menu_click(BUTTON *m) {
	WINDOW *w = (WINDOW *)m->gen->get_window((WIDGET *)m);
	if (!w) return;
	w->win->set_x(w, w->gen->get_w((WIDGET *)w) - 2*18 - 2*2 - 15);
	w->gen->update((WIDGET *)w);
}


/*** CALLBACK THAT IS CALLED WHEN USER CLICKS ON MINIMIZED MENUBAR ***/
static void smallmenu_click(BUTTON *m) {
	WINDOW *w = (WINDOW *)m->gen->get_window((WIDGET *)m);
	if (!w) return;
	w->win->set_x(w, -2 - 18 - 5);
	w->gen->update((WIDGET *)w);
}


/*** CREATE MENU BAR ***/
static void create_menubar(SCREEN *scr) {
	WINDOW    *w;
	BUTTON    *b;
	FRAME     *f;
	CONTAINER *c;
	int scr_w = gfx->get_width(scr->sd->scr_ds);

	w = win->create();
	w->win->set_elem_mask(w, 0);
	w->win->set_staytop(w, 1);
	w->gen->update((WIDGET *)w);

	f = frame->create();
	w->win->set_content(w, (WIDGET *)f);

	c = cont->create();
	c->gen->set_w((WIDGET *)c, scr_w + 4 + 18);
	c->gen->set_h((WIDGET *)c, 22);
	c->gen->updatepos((WIDGET *)c);

	scr->sd->menubutton = b = but->create();
	b->but->set_free_w(b, 1);
	b->but->set_free_h(b, 1);
	b->gen->set_x((WIDGET *)b, 18);
	b->gen->set_y((WIDGET *)b, 0);
	b->gen->set_w((WIDGET *)b, scr_w + 4);
	b->gen->set_h((WIDGET *)b, 22);
	b->gen->set_selectable((WIDGET *)b, 0);
	b->but->set_pad_x(b, 0);
	b->but->set_pad_y(b, 0);
	b->gen->updatepos((WIDGET *)b);
	b->but->set_click(b, menu_click);
	b->gen->update((WIDGET *)b);
	c->cont->add(c, (WIDGET *)b);

	b = but->create();
	b->but->set_free_w(b, 1);
	b->but->set_free_h(b, 1);
	b->gen->set_x((WIDGET *)b, 0);
	b->gen->set_y((WIDGET *)b, 2);
	b->gen->set_w((WIDGET *)b, 18);
	b->gen->set_h((WIDGET *)b, 20);
	b->but->set_pad_x(b, 0);
	b->but->set_pad_y(b, 0);
	b->gen->updatepos((WIDGET *)b);
	c->cont->add(c, (WIDGET *)b);
	b->but->set_text(b, "!");
	b->but->set_click(b, smallmenu_click);
	b->gen->update((WIDGET *)b);

	w->win->set_content(w, (WIDGET *)c);
	w->gen->update((WIDGET *)w);

	scr->scr->place(scr, (WIDGET *)w, -2 - 18 - 5, -2 - 1, scr_w + 4 + 18 + 10, 22 + 8);
}


/*** CALLBACK ROUTINE THAT IS EXECUTED WHEN USER CLICKS ON THE DESKTOP ***/
static void dummyclick(void *w) { w=w; }


/*** CREATE DESKTOP (BACKGROUND OF THE SCREEN) WINDOW ***/
static void create_desktop(SCREEN *scr) {
	WINDOW *desk;
	BACKGROUND *b;
	int scr_w = gfx->get_width(scr->sd->scr_ds);
	int scr_h = gfx->get_height(scr->sd->scr_ds);

	desk = win->create();

	b = bg->create();
	b->bg->set_style(b, BG_STYLE_DESK);
	b->bg->set_click(b, dummyclick);
	b->gen->update((WIDGET *)b);
	desk->win->set_content(desk, (WIDGET *)b);
	desk->gen->update((WIDGET *)desk);

	/* place desktop window outside of the screen */
	scr->scr->place(scr, (WIDGET *)desk, scr_w + 50, -50, scr_w + 100, scr_h + 100);
	viewman->set_bg(desk->wd->context);

	/* move desktop window to the screen area */
	scr->scr->place(scr, (WIDGET *)desk, -50, -50, scr_w + 100, scr_h + 100);
}


/*** ADD WINDOW TO WINDOW LIST ***
 *
 * \param after  window after that the new one should be inserted or
 *               NULL if window should be inserted at the beginning
 *
 * The reference counter of the new window is not touched by this function.
 */
static void chain_window(SCREEN *scr, WIDGET *win, WIDGET *after) {

	/* if insert position is specified */
	if (after && (after->wd->parent == scr)) {
		win->wd->next = after->wd->next;
		after->wd->next = win;

	/* otherwise put new window at the beginning of the list */
	} else {
		win->wd->next = scr->sd->first_win;
		scr->sd->first_win = win;
	}

	win->wd->parent = scr;
}


/*** REMOVE WINDOW FROM WINDOW LIST ***
 *
 * The reference counter of the window is not affected by this
 * function. The caller has to manage the reference counter by
 * itself.
 */
static void unchain_window(SCREEN *scr, WIDGET *win) {
	WIDGET *cw;

	/* is win the first win? */
	if (win == scr->sd->first_win)
		scr->sd->first_win = win->wd->next;

	/* search in the window list for the window */
	else for (cw = scr->sd->first_win; cw; cw = cw->wd->next) {

		/* is the next of the current list entry the window? */
		if (cw->wd->next == win) {

			/* skip win in list */
			cw->wd->next = win->wd->next;
			break;
		}
	}

	/* isolate unchained window */
	win->wd->parent = win->wd->next = NULL;
}


/*** UPDATE THE SCREEN REGION THAT BELONGS TO THE SPECIFIED WINDOW ***/
static inline void redraw_window_area(SCREEN *scr, WIDGET *win) {
	redraw->draw_area(scr, win->wd->x,  win->wd->y,
	                       win->wd->x + win->wd->w - 1,
	                       win->wd->y + win->wd->h - 1);
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

/*** FIND WIDGET AT A SPECIFIED ABSOLUTE SCREEN POSITION ***/
static WIDGET *scr_find(SCREEN *scr, long x, long y) {
	WIDGET *win = scr->sd->first_win;
	WIDGET *result;
	while (win != NULL) {
		if ((result = win->gen->find(win, x, y))) return result;
		win = win->gen->get_next(win);
	}
	return NULL;
}


/*** DRAW CONTENT AT THE SPECIFIED AREA OF THE SCREEN ***
 *
 * This function catches the drawarea requests coming from child widgets.
 */
static int (*orig_drawarea) (WIDGET *, WIDGET *, long, long, long, long);
static int scr_drawarea(SCREEN *scr, WIDGET *origin, long x, long y, long w, long h) {
	GFX_CONTAINER *ds = scr->sd->scr_ds;
	WIDGET *parent;
	
	/* is scr child of another widget? we go on with propagating the request */
	parent = scr->gen->get_parent(scr);
	if (parent) return orig_drawarea(scr, origin, x, y, w, h);

	gfx->reset_clipping(ds);

	/* if redraw request refers to the screen, reset origin */
	if (origin == scr) origin = NULL;

	return draw_rec(ds, scr->sd->first_win, origin, x, y, x + w - 1, y + h - 1, 1);
}


int transparency_depth;  /* current depth of transparency */

static int scr_drawbehind(SCREEN *scr, WIDGET *win,
                          long x, long y, long w, long h, WIDGET *origin) {
	int ret = 0;
	WIDGET *next;

	if (!win || (win->gen->get_parent(win) != scr)) return 0;
	next = win->gen->get_next(win);

	/* if maximum depth is reached, just paint a black box */
	if (transparency_depth >= 2) {
		if (!origin) win->gen->draw_bg(win, scr->sd->scr_ds, x, y, w, h, NULL, 1);
		return 0;
	}
	transparency_depth++;
	if (next) ret |= draw_rec(scr->sd->scr_ds, next, origin, x, y, x + w - 1, y + h - 1, 0);
	transparency_depth--;

	return ret;
}


static int scr_do_layout(SCREEN *s, WIDGET *child) {
	int w, h;
	
	if (child->gen->get_parent(child) != s) return 0;

	/* check if widget size must be changed to fit in its min/max range */
	w = child->wd->w;
	h = child->wd->h;
	
	if ((w >= child->wd->min_w) && (w <= child->wd->max_w)
	 && (h >= child->wd->min_h) && (h <= child->wd->max_h))
		return 0;

	if (w <= child->wd->min_w) w = child->wd->min_w;
	if (w >= child->wd->max_w) w = child->wd->max_w;
	if (h <= child->wd->min_h) h = child->wd->min_h;
	if (h >= child->wd->max_h) h = child->wd->max_h;
	
	s->scr->place(s, child, NOARG, NOARG, w, h);
	return 0;
}


/*** PROVIDE INFORMATION WHETHER THE WIDGET IS ROOT OR NOT ***
 *
 * Generally, a screen widget is a root widget - that means
 * it does not propagate redraw requests to a parent but triggers
 * the actual execution of redraw requests. There is the special
 * case if the screen is a child widget (if there is any reason
 * for this?).
 */
static int scr_is_root(SCREEN *s) {

	/* if the screen is a child it cannot be root */
	if (s->gen->get_parent(s)) return 0;

	/* otherwise, we are root */
	return 1;
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *scr_get_type(SCREEN *s) {
  (void)s;
	return "Screen";
}


/*******************************
 *** SCREEN SPECIFIC METHODS ***
 *******************************/

/*** SET GFX CONTAINER TO USE FOR SCREEN OUTPUT ***/
static void scr_set_gfx(SCREEN *scr, GFX_CONTAINER *ds) {
	scr->sd->scr_ds = ds;
	scr->wd->min_w = scr->wd->max_w = scr->wd->w = gfx->get_width(ds);
	scr->wd->min_h = scr->wd->max_h = scr->wd->h = gfx->get_height(ds);

	/*
	 * Now we know the size of the gfx container,
	 * we can create a menubar and desktop
	 */

	if (config_menubar)
		create_menubar(scr);

	create_desktop(scr);
}


/*** SET ACTIVE WINDOW ***/
static void scr_set_act_win(SCREEN *scr, WIDGET *w) {
	WINDOW *new = (WINDOW *)w, *old;
	BUTTON *b;
	char *new_txt = "";

	if (new == (WINDOW *)scr->sd->active_win) return;

	if (new) {
		if (new->win->get_staytop(new)) return;
		new->gen->inc_ref((WIDGET *)new);
		new->win->set_state(new, 1);
	}

	if ((old = (WINDOW *)scr->sd->active_win)) {
		old->win->set_state(old, 0);
		old->gen->dec_ref((WIDGET *)old);
	}

	scr->sd->active_win = (WIDGET *)new;

	/* use title of new focused application for menu bar */
	if (new) {
		int app_id = new->gen->get_app_id((WIDGET *)new);
		new_txt    = appman->get_app_name(app_id);
	}

	/* set new text in menu bar */
	if ((b = scr->sd->menubutton)) {
		b->but->set_text(b, new_txt);
		b->gen->update((WIDGET *)b);
	}
}


/*** ADD WINDOW TO THE WINDOW DISPLAY LIST ***/
static void scr_place(SCREEN *scr, WIDGET *ww, long x, long y, long w, long h) {
	int ox1, oy1, ox2, oy2;
	int nx1, ny1, nx2, ny2;

	/* bad child */
	if (!ww) return;

	/* avoid cyclic parent relationships */
	if (ww->gen->related_to(ww, scr)) return;

	ox1 = ww->gen->get_x(ww);
	oy1 = ww->gen->get_y(ww);
	ox2 = ww->gen->get_w(ww) + ox1 - 1;
	oy2 = ww->gen->get_h(ww) + oy1 - 1;
	
	/* determine desired new position */
	if (x == NOARG) x = ww->gen->get_x(ww);
	if (y == NOARG) y = ww->gen->get_y(ww);
	if (w == NOARG) w = ww->gen->get_w(ww);
	if (h == NOARG) h = ww->gen->get_h(ww);

	if (w < ww->gen->get_min_w(ww)) w = ww->gen->get_min_w(ww);
	if (w > ww->gen->get_max_w(ww)) w = ww->gen->get_max_w(ww);
	if (h < ww->gen->get_min_h(ww)) h = ww->gen->get_min_h(ww);
	if (h > ww->gen->get_max_h(ww)) h = ww->gen->get_max_h(ww);
	
	ww->gen->set_x(ww, x);
	ww->gen->set_y(ww, y);
	ww->gen->set_w(ww, w);
	ww->gen->set_h(ww, h);
	ww->gen->updatepos(ww);

	/* create a view if this window is new */
	if (!ww->wd->context) ww->wd->context = viewman->create();
	if (ww->wd->context)
		viewman->set_title(ww->wd->context,
		                   appman->get_app_name(ww->gen->get_app_id(ww)));

	/* update view */
	viewman->place(ww->wd->context, x + win->shadow_left, y + win->shadow_top,
	                                w - win->shadow_left - win->shadow_right,
	                                h - win->shadow_top - win->shadow_bottom);

	nx1 = ww->gen->get_x(ww);
	ny1 = ww->gen->get_y(ww);
	nx2 = ww->gen->get_w(ww) + nx1 - 1;
	ny2 = ww->gen->get_h(ww) + ny1 - 1;

	/* check if we adopted this window... dont make this mistake again */
	if (ww->gen->get_parent(ww) == scr) {
		redraw->draw_area(scr, MIN(ox1, nx1), MIN(oy1, ny1), MAX(ox2, nx2), MAX(oy2, ny2));
		return;
	}

	/* add window to the window list at the first possible position */
	chain_window(scr, ww, get_last_staytop_win(scr));
	ww->gen->inc_ref(ww);

	/* redraw the new window... */
	redraw_window_area(scr, ww);
}


/*** REMOVE WINDOW FROM THE WINDOW DISPLAY LIST ***/
static void scr_remove(SCREEN *scr, WIDGET *win) {
	WIDGET *cw;

	if (!win || (win->gen->get_parent(win) != scr)) return;

	/* is the window the currently active one? */
	if (scr->sd->active_win == win)
		scr_set_act_win(scr, NULL);

	/* destroy view of the window at the view manager */
	viewman->destroy(win->wd->context);
	win->wd->context = 0;

	/* dissolve the relationship to userstate manager */
	if ((cw = userstate->get_selected()) && (cw->gen->get_window(cw) == win))
		userstate->idle();

	/* remove window from window list */
	unchain_window(scr, win);

	/* redraw area where the window was before we kicked it out... */
	redraw_window_area(scr, win);

	/* forget about the dropped window */
	win->gen->dec_ref(win);
}


/*** PULL WINDOW TO THE TOP WHILE RESPECTING 'STAYTOP'-WINDOWS ***/
static void scr_top(SCREEN *scr, WIDGET *win) {

	if (!win || (win->wd->parent != scr)) return;

	/* check if userstate is valid */
	if (userstate->get() == USERSTATE_GRAB) userstate->idle();

	/* put window to the begin of the window list */
	unchain_window(scr, win);
	chain_window(scr, win, get_last_staytop_win(scr));

	/* notify view manager */
	viewman->top(win->wd->context);

	/* redraw window area */
	redraw_window_area(scr, win);
}


/*** PUT WINDOW TO THE BACKGROUND ***/
static void scr_back(SCREEN *scr, WIDGET *win) {
	WIDGET *cw;

	if (!win || (win->wd->parent != scr)) return;

	/* check if userstate is valid */
	if (userstate->get() == USERSTATE_GRAB) userstate->idle();

	/* put window to the begin of the window list */
	unchain_window(scr, win);

	/* search for window position before DOpE default background */
	cw = scr->sd->first_win;
	for (; cw && cw->wd->next && cw->wd->next->wd->next; cw = cw->wd->next);

	if (cw == scr->sd->first_win && !scr->sd->first_win->wd->next)
	  cw = NULL;

	/* insert window at the tail of the window list */
	chain_window(scr, win, cw);

	/* notify view manager */
	viewman->back(win->wd->context);
	viewman->set_bg(win->wd->context);

	/* redraw window area */
	redraw_window_area(scr, win);
}


/*** REGISTER WINDOW TITLE ***/
static void scr_set_title(SCREEN *scr, WIDGET *win, char *title) {

	if (!win || (win->wd->parent != scr)) return;

	/* notify view manager */
	viewman->set_title(win->wd->context, title);
}


/*** PULL ALL 'STAYTOP'-WINDOWS TO THE BEGIN OF THE WINDOW STACK ***/
static void scr_reorder(SCREEN *scr) {
	WINDOW *cw = (WINDOW *)scr->sd->first_win, *stw;

	/* find first 'normal' window */
	while (cw) {
		if (!cw->win->get_staytop(cw)) {

			/* is there a 'staytop' window after it? */
			stw = (WINDOW *)cw->gen->get_next((WIDGET *)cw);
			while (stw) {
				if (stw->win->get_staytop(stw)) scr_top(scr, (WIDGET *)stw);
				stw = (WINDOW *)stw->gen->get_next((WIDGET *)stw);
			}
			cw = (WINDOW *)cw->gen->get_next((WIDGET *)cw);
		}
	}
}


/*** RETURN WIDTH OF THE SCREEN ***/
static long scr_get_w(SCREEN *s) {
	if (!s || !s->sd->scr_ds) return 0;
	return gfx->get_width(s->sd->scr_ds);
}


/*** RETURN HEIGHT OF THE SCREEN ***/
static long scr_get_h(SCREEN *s) {
	if (!s || !s->sd->scr_ds) return 0;
	return gfx->get_height(s->sd->scr_ds);
}


static struct widget_methods gen_methods;
static struct screen_methods scr_methods = {
	scr_set_gfx,
	scr_place,
	scr_remove,
	scr_top,
	scr_back,
	scr_set_title,
	scr_reorder,
	scr_set_act_win,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static SCREEN *create(void) {
	SCREEN *new = ALLOC_WIDGET(struct screen);
	SET_WIDGET_DEFAULTS(new, struct screen, &scr_methods);

	/* insert screen into screen list */
	if (!first_scr)
		first_scr = new;
	else {
		new->sd->next = first_scr->sd->next;
		first_scr = new;
	}
	return new;
}


/*** REMOVE ALL CHILDREN OF THE SPECIFIED APPLICATION FROM ALL SCREENS ***/
static void forget_children(int app_id) {
	SCREEN *cs = first_scr;
	while (cs) {
		WIDGET *cw;
		SCREEN *ns;

		cs->gen->lock(cs);

		/* check if active win belongs to the application */
		if (cs->sd->active_win
		 && (cs->sd->active_win->gen->get_app_id(cs->sd->active_win) == app_id))
			scr_set_act_win(cs, NULL);

		for (cw = cs->sd->first_win; cw; ) {
			WIDGET *nw = cw->gen->get_next(cw);
			if (cw->gen->get_app_id(cw) == app_id)
				cs->scr->remove(cs, cw);
			cw = nw;
		}

		ns = cs->sd->next;
		cs->gen->unlock(cs);
		cs = ns;
	}
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct screen_services services = {
	create,
	forget_children,
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;
	widtype = script->reg_widget_type("Screen", (void *(*)(void))create);
	script->reg_widget_attrib(widtype, "long w", scr_get_w, NULL, NULL);
	script->reg_widget_attrib(widtype, "long h", scr_get_h, NULL, NULL);
	widman->build_script_lang(widtype, &gen_methods);
}


int init_screen(struct dope_services *d) {

	userstate = d->get_module("UserState 1.0");
	viewman   = d->get_module("ViewManager 1.0");
	widman    = d->get_module("WidgetManager 1.0");
	script    = d->get_module("Script 1.0");
	appman    = d->get_module("ApplicationManager 1.0");
	redraw    = d->get_module("RedrawManager 1.0");
	frame     = d->get_module("Frame 1.0");
	gfx       = d->get_module("Gfx 1.0");
	cont      = d->get_module("Container 1.0");
	but       = d->get_module("Button 1.0");
	win       = d->get_module("Window 1.0");
	bg        = d->get_module("Background 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);
	orig_drawarea = gen_methods.drawarea;
	gen_methods.get_type   = scr_get_type;
	gen_methods.find       = scr_find;
	gen_methods.drawarea   = scr_drawarea;
	gen_methods.is_root    = scr_is_root;
	gen_methods.do_layout  = scr_do_layout;
	gen_methods.drawbehind = scr_drawbehind;

	build_script_lang();

	d->register_module("Screen 1.0", &services);
	return 1;
}
