/*
 * \brief   DOpE Entry widget module
 * \date    2004-04-07
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

struct entry;
#define WIDGET struct entry

#include "dopestd.h"
#include "entry.h"
#include "gfx_macros.h"
#include "widget_data.h"
#include "widget_help.h"
#include "fontman.h"
#include "script.h"
#include "widman.h"
#include "userstate.h"
#include "messenger.h"
#include "keycodes.h"
#include "relax.h"
#include "tick.h"

static struct widman_services    *widman;
static struct gfx_services       *gfx;
static struct fontman_services   *font;
static struct script_services    *script;
static struct userstate_services *userstate;
static struct relax_services     *relax;
static struct messenger_services *msg;
static struct tick_services      *tick;

struct entry_data {
	s16    font_id;                  /* used font                      */
	s16    flags;                    /* entry properties               */
	RELAX  tx;                       /* adaptive text position         */
	s16    ty;                       /* text position inside the entry */
	s32    tw, th;                   /* pixel width and height of text */
	s32    sel_beg, sel_end;         /* current selection              */
	s32    sel_x, sel_w;             /* pixel position of selection    */
	s32    curpos;                   /* position of cursor             */
	s32    cx, ch;                   /* cursor x position and height   */
	s16    pad_x, pad_y;             /* padding aroung entry           */
	char  *txtbuf;                   /* textual content of the entry   */
	s32    txtbuflen;                /* current size of text buffer    */
	s32    maxlen;                   /* max string length              */
	s32    vislen;                   /* min visible length             */
	void (*click)  (WIDGET *);
	void (*release)(WIDGET *);
};

#define ENTRY_FLAGS_BLIND 0x1   /* blind out characters */

static GFX_CONTAINER *normal_img;
static GFX_CONTAINER *focus_img;

int init_entry(struct dope_services *d);

#define BLACK_SOLID GFX_RGBA(0, 0, 0, 255)
#define BLACK_MIXED GFX_RGBA(0, 0, 0, 127)
#define WHITE_SOLID GFX_RGBA(255, 255, 255, 255)
#define WHITE_MIXED GFX_RGBA(255, 255, 255, 127)
#define DGRAY_MIXED GFX_RGBA(80,  80,   80, 127)
#define MGRAY_MIXED GFX_RGBA(127, 127, 127, 127)
#define LGRAY_MIXED GFX_RGBA(148, 148, 148, 127)


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** DELETE CHARACTER AT SPECIFIED STRING POSITION ***
 *
 * \return   1 on success
 */
static int delete_char(ENTRY *e, unsigned idx) {
	char *dst = e->ed->txtbuf + idx;
	char *src = e->ed->txtbuf + idx + 1;
	if (idx >= strlen(e->ed->txtbuf)) return 0;
	while (*src) *(dst++) = *(src++);
	*dst = 0;
	return 1;
}


/*** INSERT CHARACTER AT SPECIFIED STRING POSITION ***
 *
 * If the string buffer exceeds, a new one is allocated.
 *
 * \return   1 on success
 */
static int insert_char(ENTRY *e, int idx, char c) {
	int i, len = strlen(e->ed->txtbuf);
	char *src, *dst;

	/* if txtbuffer exceeds, reallocate a bigger one */
	if (len >= e->ed->txtbuflen) {
		char *new;
		e->ed->txtbuflen = e->ed->txtbuflen * 2 + 1;
		new = (char *)zalloc(e->ed->txtbuflen);
		if (!new) return 0;
		for(i=0; i<=len; i++) new[i] = e->ed->txtbuf[i];
		free(e->ed->txtbuf);
		e->ed->txtbuf = new;
	}
	dst = e->ed->txtbuf + len;
	src = dst - 1;

	/* move tail of string to the right */
	for (i=0; i<len-idx; i++) *(dst--) = *(src--);

	/* insert character */
	*dst = c;
	return 1;
}


static int tick_relax_tx(void *arg) {
	ENTRY *e = (ENTRY *)arg;

	if (!relax->do_relax(&e->ed->tx)) {
		e->gen->dec_ref(e);
		return 0;   /* stop ticking */
	}
	e->gen->force_redraw(e);
	return 1;   /* keep ticking */
}


/*** CALCULATE X POSITION OF THE CHARACTER AT THE SPECIFIED INDEX ***/
static inline int get_char_pos(ENTRY *e, int idx) {
	int xpos, tmp = e->ed->txtbuf[idx];

	if (e->ed->flags & ENTRY_FLAGS_BLIND)
		return idx*font->calc_str_width(e->ed->font_id, "*");

	e->ed->txtbuf[idx] = 0;
	xpos = font->calc_str_width(e->ed->font_id, e->ed->txtbuf);
	e->ed->txtbuf[idx] = tmp;
	return xpos;
}


/*** CALCULATE CHARACTER INDEX THAT CORRESPONDS TO THE SPECIFIED POSITION ***/
static inline int get_char_index(ENTRY *e, int pos) {
	if (e->ed->flags & ENTRY_FLAGS_BLIND) {
		int step = font->calc_str_width(e->ed->font_id, "*");
		unsigned res = step ? pos/step : 0;
		if (res > strlen(e->ed->txtbuf)) res = strlen(e->ed->txtbuf);
		return res;
	}
	return font->calc_char_idx(e->ed->font_id, e->ed->txtbuf, pos);
}


/*** CALCULATE TEXT AND CURSOR PIXEL POSITIONS ***/
static void update_text_pos(ENTRY *e) {
	int vw;
	if (!e || !e->ed || !e->ed->txtbuf) return;
	
	e->ed->ty = 0;
	e->ed->tw = font->calc_str_width (e->ed->font_id, e->ed->txtbuf);
	e->ed->th = font->calc_str_height(e->ed->font_id, e->ed->txtbuf);
	e->ed->ch = e->ed->th;

	/* calculate x position of cursor */
	e->ed->cx = get_char_pos(e, e->ed->curpos);

	if (e->ed->sel_beg < e->ed->sel_end) {
		e->ed->sel_x = get_char_pos(e, e->ed->sel_beg);
		e->ed->sel_w = get_char_pos(e, e->ed->sel_end) - e->ed->sel_x + 1;
	} else if (e->ed->sel_beg > e->ed->sel_end) {
		e->ed->sel_x = get_char_pos(e, e->ed->sel_end);
		e->ed->sel_w = get_char_pos(e, e->ed->sel_beg) - e->ed->sel_x + 1;
	} else {
		e->ed->sel_x = e->ed->sel_w = 0;
	}

	/* set text position so that the cursor is visible */
	e->ed->tx.dst = 0;
	vw = e->wd->w - 2*e->ed->pad_x - 6;   /* visible width */

	if ((e->ed->tx.dst > vw - e->ed->tw) && (e->ed->tx.dst > vw + 2 - (vw/3) - e->ed->cx)) {
		e->ed->tx.dst = vw - (vw/3) - e->ed->cx;
		if (e->ed->tx.dst < vw - e->ed->tw)
			e->ed->tx.dst = vw - e->ed->tw;
	}
	
	if (e->ed->tx.dst <    - e->ed->cx) e->ed->tx.dst =    - e->ed->cx;
	if (e->ed->tx.dst > vw - e->ed->cx) e->ed->tx.dst = vw - e->ed->cx;
}


/*** IF TEXT POSITION IS NOT THE DESIRED TEXT POSITION - MOVE! ***/
static void start_relax(ENTRY *e) {
	relax->set_duration(&e->ed->tx, 15);
	if (e->ed->tx.curr == e->ed->tx.dst || e->ed->tx.speed) return;

	e->gen->inc_ref(e);
	e->ed->tx.speed = 1;

	if (!tick->add(25, tick_relax_tx, e))
		e->ed->tx.curr = e->ed->tx.dst;
}


static inline void draw_sunken_frame(GFX_CONTAINER *d, s32 x, s32 y, s32 w, s32 h) {

	/* outer frame */
	gfx->draw_hline(d, x + 1,     y,         w - 2, BLACK_MIXED);
	gfx->draw_vline(d, x,         y,         h,     BLACK_MIXED);
	gfx->draw_hline(d, x + 1,     y + h - 1, w - 2, BLACK_MIXED);
	gfx->draw_vline(d, x + w - 1, y,         h,     BLACK_MIXED);
	
	/* inner frame */
	gfx->draw_hline(d, x + 1, y + 1, w - 2, DGRAY_MIXED);
	gfx->draw_vline(d, x + 1, y + 1, h - 2, DGRAY_MIXED);
	gfx->draw_hline(d, x + 2, y + 2, w - 3, MGRAY_MIXED);
	gfx->draw_vline(d, x + 2, y + 2, h - 3, MGRAY_MIXED);
	gfx->draw_hline(d, x + 3, y + 3, w - 4, LGRAY_MIXED);
	gfx->draw_vline(d, x + 3, y + 3, h - 4, LGRAY_MIXED);
}


static inline void draw_kfocus_frame(GFX_CONTAINER *d, s32 x, s32 y, s32 w, s32 h) {

	gfx->draw_hline(d, x + 1,     y,         w - 2, BLACK_SOLID);
	gfx->draw_vline(d, x,         y,         h,     BLACK_SOLID);
	gfx->draw_hline(d, x + 1,     y + h - 1, w - 2, BLACK_SOLID);
	gfx->draw_vline(d, x + w - 1, y,         h,     BLACK_SOLID);
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

static int entry_draw(ENTRY *e, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	long tx = e->ed->tx.curr, ty = e->ed->ty;
	u32  tc = GFX_RGB(32, 32, 32);
	u32  cc = BLACK_MIXED;
	s32  cx;
	long w  = e->wd->w,  h  = e->wd->h;

	if (origin == e) return 1;
	if (origin) return 0;

	x += e->wd->x;
	y += e->wd->y;

	gfx->push_clipping(ds, x, y, w, h);

	x += e->ed->pad_x;
	y += e->ed->pad_y;
	w -= e->ed->pad_x*2;
	h -= e->ed->pad_y*2;

	if (e->wd->flags & WID_FLAGS_KFOCUS)
		draw_kfocus_frame(ds, x - 1, y - 1, w + 2, h + 2);

	if (e->wd->flags & WID_FLAGS_MFOCUS)
		gfx->draw_img(ds, x, y, w, h, focus_img, 255);
	else
		gfx->draw_img(ds, x, y, w, h, normal_img, 255);

	draw_sunken_frame(ds, x, y, w, h);

	if (e->wd->flags & WID_FLAGS_MFOCUS) tc = cc = BLACK_SOLID;
	
	tx += x + 3; ty += y + 2;

	gfx->push_clipping(ds, x+2, y+2, w-3, h-3);

	/* draw selection */
	if (e->ed->sel_w)
		gfx->draw_box(ds, e->ed->sel_x + tx - 1, ty, e->ed->sel_w, e->ed->ch+1, GFX_RGBA(127,127,127,127));
	
	if (e->ed->txtbuf) {
		if (e->ed->flags & ENTRY_FLAGS_BLIND) {
			int step = font->calc_str_width(e->ed->font_id, "*");
			int i, len = strlen(e->ed->txtbuf);
			for (i = 0; i < len; i++) {
				gfx->draw_string(ds, tx + step*i, ty, tc, 0, e->ed->font_id, "*");
			}
		} else gfx->draw_string(ds, tx, ty, tc, 0, e->ed->font_id, e->ed->txtbuf);
	}

	/* draw cursor */
	if (e->wd->flags & WID_FLAGS_KFOCUS) {
		cx = tx + e->ed->cx - 2;
		gfx->draw_vline(ds, cx + 1, ty, e->ed->ch, cc);
		gfx->draw_hline(ds, cx, ty, 3, cc);
		gfx->draw_hline(ds, cx, ty + e->ed->ch - 1, 3, cc);
	}

	gfx->pop_clipping(ds);
	gfx->pop_clipping(ds);
	return 1;
}


static ENTRY *entry_find(ENTRY *e, long x, long y) {
	x -= e->wd->x;
	y -= e->wd->y;
	
	/* check if position is inside the entry */
	if ((x >= e->ed->pad_x) && (x < e->wd->w - e->ed->pad_x)
	 && (y >= e->ed->pad_y) && (y < e->wd->h - e->ed->pad_y))
		return e;

	return NULL;
}


static void sel_tick(ENTRY *e, int dx, int dy) {
  (void)dx; (void)dy;
	int mx = userstate->get_mx();
	int lx = e->gen->get_abs_x(e);
	int xpos = mx - lx - e->ed->tx.curr - e->ed->pad_x - 3;
	int csel;
	int vw = e->wd->w - 2*e->ed->pad_x - 6;

	/* mouse beyond left widget border - scroll text area */
	if (mx < lx) {
		e->ed->tx.curr += (lx - mx)/4;
		if (e->ed->tx.curr > 0) e->ed->tx.curr = 0;
		xpos = -e->ed->tx.curr;
	}
	/* mouse beyond right widget border - scroll text area */
	if (mx > lx + e->wd->w) {
		e->ed->tx.curr += (lx + e->wd->w - mx)/4;
		if (e->ed->tx.curr + e->ed->tw < vw) e->ed->tx.curr = vw - e->ed->tw;
		if (e->ed->tx.curr > 0) e->ed->tx.curr = 0;
		xpos = vw - e->ed->tx.curr;
	}
	
	csel = get_char_index(e, xpos);
	e->ed->sel_end = csel;
	
	update_text_pos(e);
	e->gen->force_redraw(e);
}


static void sel_release(ENTRY *e, int dx, int dy) {
  (void)dx; (void)dy;
	e->ed->curpos = e->ed->sel_end;
	update_text_pos(e);
	start_relax(e);
}


static void (*orig_handle_event) (ENTRY *e, EVENT *ev, WIDGET *from);
static void entry_handle_event(ENTRY *e, EVENT *ev, WIDGET *from) {
	int xpos = userstate->get_mx() - e->gen->get_abs_x(e);
	int ascii;
	int ev_done = 0;

	if (ev->type != L4RE_EV_KEY)
	  return;

	switch (ev->value) {
	case 1: // PRESS
	case 2: // REPEAT
		e->ed->sel_beg = e->ed->sel_end = e->ed->sel_w = 0;
		switch (ev->code) {
			case L4RE_BTN_MOUSE:
				xpos -= e->ed->tx.curr + e->ed->pad_x + 3;
				e->ed->curpos = get_char_index(e, xpos);
				e->ed->sel_beg = e->ed->sel_end = e->ed->curpos;
				e->ed->sel_w = 0;
				userstate->drag(e, NULL, sel_tick, sel_release);
				ev_done = 1;
				break;
			case L4RE_KEY_LEFT:
				if (e->ed->curpos > 0) e->ed->curpos--;
				ev_done = 2;
				break;
				
			case L4RE_KEY_RIGHT:
				if ((unsigned)e->ed->curpos < strlen(e->ed->txtbuf)) e->ed->curpos++;
				ev_done = 2;
				break;

			case L4RE_KEY_HOME:
				e->ed->curpos = 0;
				ev_done = 2;
				break;

			case L4RE_KEY_END:
				e->ed->curpos = strlen(e->ed->txtbuf);
				ev_done = 2;
				break;

			case L4RE_KEY_DELETE:
				if ((unsigned)e->ed->curpos < strlen(e->ed->txtbuf))
					delete_char(e, e->ed->curpos);
				ev_done = 2;
				break;

			case L4RE_KEY_BACKSPACE:
				if (e->ed->curpos > 0) {
					e->ed->curpos--;
					delete_char(e, e->ed->curpos);
				}
				ev_done = 2;
				break;

			case L4RE_KEY_ENTER:

				/* send commit event to client application */
				{
					unsigned long m = e->gen->get_bind_msg(e, "commit");
					int app_id = e->gen->get_app_id(e);
					if (m) msg->send_action_event(app_id, "commit", m);
				}
				ev_done = 1;
				break;

			case L4RE_KEY_TAB:
				orig_handle_event(e, ev, from);
				return;
		}

		if (ev_done) {
			update_text_pos(e);
			if (ev_done == 2) start_relax(e);
			e->gen->force_redraw(e);
			return;
		}

		/* insert ASCII character */
		ascii = userstate->get_ascii(ev->code);
		if (!ascii) return;
		insert_char(e, e->ed->curpos, ascii);
		e->ed->curpos++;
		e->wd->update |= WID_UPDATE_REFRESH;
		e->gen->update(e);
	}
}


/*** DETERMINE MIN/MAX SIZE OF A ENTRY ***
 *
 * The height and the min width of a Entry depend on the used font and
 * the Entry text.
 */
static void entry_calc_minmax(ENTRY *e) {
	e->wd->min_w = e->ed->pad_x*2 + 4 + e->ed->vislen*font->calc_str_width(e->ed->font_id, "W");
	e->wd->max_w = 99999;
	e->wd->min_h = e->wd->max_h = font->calc_str_height(e->ed->font_id, "W")
	                            + e->ed->pad_y*2 + 4;
}


/*** UPDATE POSITION AND SIZE OF ENTRY ***
 *
 * When size changes, the text position must be updated.
 */
static void (*orig_updatepos) (ENTRY *e);
static void entry_updatepos(ENTRY *e) {
	update_text_pos(e);
	orig_updatepos(e);
	start_relax(e);
}


static void (*orig_update) (ENTRY *e);
static void entry_update(ENTRY *e) {
	orig_update(e);
	start_relax(e);
}


/*** FREE ENTRY WIDGET DATA ***/
static void entry_free_data(ENTRY *e) {
	if (e->ed->txtbuf) free(e->ed->txtbuf);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *entry_get_type(ENTRY *e) {
  (void)e;
	return "Entry";
}


/*******************************
 *** ENTRY SPECIFIC METHODS ***
 *******************************/

static void entry_set_text(ENTRY *e, char *new_txt) {
	int i;

	if (!new_txt) return;

	/* clear old text */
	while (e->ed->txtbuf[0]) delete_char(e, 0);

	/* insert new charaters */
	for (i = 0; new_txt[i]; i++) insert_char(e, i, new_txt[i]);

	/* make the new text visible */
	e->wd->update |= WID_UPDATE_REFRESH;
}


static char *entry_get_text(ENTRY *e) {
	if (!e || !e->ed) return 0;
	return e->ed->txtbuf;
}


static void entry_set_blind(ENTRY *e, long blind_flag) {

	if (blind_flag)
		e->ed->flags |= ENTRY_FLAGS_BLIND;
	else
		e->ed->flags &= ~ENTRY_FLAGS_BLIND;

	e->wd->update |= WID_UPDATE_REFRESH;
}


static int entry_get_blind(ENTRY *e) {
	return !!(e->ed->flags & ENTRY_FLAGS_BLIND);
}


static struct widget_methods gen_methods;
static struct entry_methods entry_methods = {
	entry_set_text,
	entry_get_text,
};



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static ENTRY *create(void) {

	ENTRY *new = ALLOC_WIDGET(struct entry);
	SET_WIDGET_DEFAULTS(new, struct entry, &entry_methods);

	/* set entry specific attributes */
	new->ed->pad_x     = new->ed->pad_y = 2;
	new->ed->vislen    = 2;
	new->ed->sel_beg   = -1;
	new->ed->sel_end   = -1;
	new->ed->txtbuflen = 16;
	new->ed->txtbuf    = zalloc(new->ed->txtbuflen);
	new->wd->flags    |= WID_FLAGS_EDITABLE | WID_FLAGS_HIGHLIGHT;

	/* let the entry receive the keyboard focus even without any bindings */
	new->wd->flags    |= WID_FLAGS_TAKEFOCUS;

	update_text_pos(new);
	entry_calc_minmax(new);
	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct entry_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("Entry", (void *(*)(void))create);
	script->reg_widget_attrib(widtype, "string text", entry_get_text, entry_set_text, gen_methods.update);
	script->reg_widget_attrib(widtype, "boolean blind", entry_get_blind, entry_set_blind, gen_methods.update);
	widman->build_script_lang(widtype, &gen_methods);
}


int init_entry(struct dope_services *d) {

	widman    = d->get_module("WidgetManager 1.0");
	gfx       = d->get_module("Gfx 1.0");
	font      = d->get_module("FontManager 1.0");
	script    = d->get_module("Script 1.0");
	userstate = d->get_module("UserState 1.0");
	msg       = d->get_module("Messenger 1.0");
	tick      = d->get_module("Tick 1.0");
	relax     = d->get_module("Relax 1.0");

	normal_img = gen_range_img(gfx, 85, 85, 85, 148, 148, 148);
	focus_img  = gen_range_img(gfx, 85, 85, 85, 162, 162, 162);

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_updatepos    = gen_methods.updatepos;
	orig_update       = gen_methods.update;
	orig_handle_event = gen_methods.handle_event;

	gen_methods.get_type     = entry_get_type;
	gen_methods.draw         = entry_draw;
	gen_methods.updatepos    = entry_updatepos;
	gen_methods.handle_event = entry_handle_event;
	gen_methods.calc_minmax  = entry_calc_minmax;
	gen_methods.find         = entry_find;
	gen_methods.update       = entry_update;
	gen_methods.free_data    = entry_free_data;

	build_script_lang();

	d->register_module("Entry 1.0", &services);
	return 1;
}
