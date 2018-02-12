/*
 * \brief   DOpE VScreen widget module
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

struct vscreen;
#define WIDGET struct vscreen

#include "dopestd.h"
#include "appman.h"
#include "thread.h"
#include "scheduler.h"
#include "widget_data.h"
#include "widget_help.h"
#include "gfx.h"
#include "redraw.h"
#include "vscreen.h"
#include "vscr_server.h"
#include "fontman.h"
#include "script.h"
#include "widman.h"
#include "userstate.h"
#include "messenger.h"
#include "keycodes.h"
#include "window.h"

#include <l4/re/env.h>

#define MAX_IDENTS 40

#define VSCR_MOUSEMODE_FREE    0
#define VSCR_MOUSEMODE_GRAB    1
#define VSCR_MOUSEMODE_GRABBED 2

#define VSCR_UPDATE_MOUSE_X   0x1
#define VSCR_UPDATE_MOUSE_Y   0x2

static struct scheduler_services   *sched;
static struct widman_services      *widman;
static struct script_services      *script;
static struct redraw_services      *redraw;
static struct appman_services      *appman;
static struct gfx_services         *gfx;
static struct messenger_services   *msg;
static struct userstate_services   *userstate;
static struct thread_services      *thread;
static struct vscr_server_services *vscr_server;
static struct fontman_services     *font;

struct vscreen_data {
	long    update_flags;
	char   *server_ident;        /* associated vscreen server identifier */
	THREAD *server_thread;
	MUTEX  *sync_mutex;
	s8      bpp;                 /* bits per pixel                               */
	s32     xres, yres;          /* virtual screen dimensions                    */
	s16     fps;                 /* frames per second                            */
	char    smb_ident[64];       /* shared memory block identifier               */
	void   *pixels;              /* pointer to page aligned pixel buffer         */
	GFX_CONTAINER *image;        /* image representation (for drawing)           */
	s16     grabmouse;           /* mouse grab mode flag 0=free 1=grab 2=grabbed */
	VSCREEN *share_next;         /* next vscreen widget with shared buffer       */
	s32     vw, vh;              /* view size                                    */
	s32     curr_vx, curr_vy;    /* current view position                        */
	s32     next_vx, next_vy;    /* next view position                           */
};

static int msg_cnt, msg_fid;    /* fade cnt and font of on-screen msg */
static int msg_x, msg_y;        /* position of onscreen message       */
static int msg_w, msg_h;        /* size of onscreen message area      */
static char *msg_txt;           /* text of the onscreen message       */

static int vs_mx, vs_my;        /* mouse position of grabbed mouse */

int init_vscreen(struct dope_services *d);


/*************************
 *** UTILITY FUNCTIONS ***
 *************************/

/*** EXCLUDE VSCREEN WIDGET FROM RING LIST OF VSCREENS WITH A SHARED BUFFER ***/
static void vscr_share_exclude(VSCREEN *vs) {
	VSCREEN *prev = vs->vd->share_next;
	while ((prev) && (prev->vd->share_next!=vs)) prev = prev->vd->share_next;
	if (prev) prev->vd->share_next = vs->vd->share_next;
	vs->vd->share_next = NULL;

	if (vs->vd->image) {
		gfx->dec_ref(vs->vd->image);
		vs->vd->image = NULL;
	}
}


/*** INCLUDE NEW VSCREEN IN RING LIST OF VSCREENS WITH A SHARED BUFFER ***
 *
 * \param vs   member of the new desired ring list
 * \param new  new vscreen widget to join the list
 */
static void vscr_share_join(VSCREEN *vs, VSCREEN *new) {
	if (new->vd->share_next) vscr_share_exclude(new);
	new->vd->share_next = vs->vd->share_next;
	vs->vd->share_next = new;
	if (!new->vd->share_next) new->vd->share_next = vs;
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/

/*** DRAW VSCREEN WIDGET ***/
static int vscr_draw(VSCREEN *vs, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	x += vs->wd->x;
	y += vs->wd->y;

	if (origin == vs) return 1;
	if (origin) return 0;

	gfx->push_clipping(ds, x, y, vs->wd->w, vs->wd->h);
	if (vs->vd->image) {
		float xratio = (float)vs->wd->w / (float)vs->vd->vw;
		float yratio = (float)vs->wd->h / (float)vs->vd->vh;
		int sx = xratio * vs->vd->curr_vx;
		int sy = yratio * vs->vd->curr_vy;
		int sw = xratio * vs->vd->xres;
		int sh = yratio * vs->vd->yres;

		gfx->draw_img(ds, x - sx, y - sy, sw, sh, vs->vd->image, 255);
	}
	if ((vs->vd->grabmouse == VSCR_MOUSEMODE_GRABBED) && (msg_cnt)) {
		int v = msg_cnt;
		gfx->draw_string(ds, x+msg_x-1, y+msg_y, GFX_RGB(0, 0, 0), 0, msg_fid, msg_txt);
		gfx->draw_string(ds, x+msg_x+1, y+msg_y, GFX_RGB(0, 0, 0), 0, msg_fid, msg_txt);
		gfx->draw_string(ds, x+msg_x, y+msg_y-1, GFX_RGB(0, 0, 0), 0, msg_fid, msg_txt);
		gfx->draw_string(ds, x+msg_x, y+msg_y+1, GFX_RGB(0, 0, 0), 0, msg_fid, msg_txt);
		gfx->draw_string(ds, x+msg_x, y+msg_y,   GFX_RGB(v, v, v), 0, msg_fid, msg_txt);
	}
	gfx->pop_clipping(ds);

	return 1;
}


static void (*orig_update) (VSCREEN *vs);

/*** UPDATE WIDGET AFTER CHANGES OF ITS ATTRIBUTES ***/
static void vscr_update(VSCREEN *vs)
{
  EVENT event;

  if (vs->vd->update_flags & (VSCR_UPDATE_MOUSE_X | VSCR_UPDATE_MOUSE_Y))
    {
      if (!(vs->vd->update_flags & VSCR_UPDATE_MOUSE_X))
	vs_mx = userstate->get_mx();

      if (!(vs->vd->update_flags & VSCR_UPDATE_MOUSE_Y))
	vs_my = userstate->get_my();

      event.time  = l4_kip_clock(l4re_kip());
      event.type  = L4RE_EV_ABS;
      event.code  = L4RE_ABS_X;
      event.value = vs_mx - vs->gen->get_abs_x(vs);
      vs->gen->handle_event(vs, &event, NULL);

      event.type  = L4RE_EV_ABS;
      event.code  = L4RE_ABS_Y;
      event.value = vs_my - vs->gen->get_abs_y(vs);
      vs->gen->handle_event(vs, &event, NULL);

      /* warp mouse to new position and handle the synthetic event */
      userstate->set_pos(vs_mx, vs_my);
  }
  orig_update(vs);
  vs->vd->update_flags = 0;
}


/*** TIMER TICK CALLBACK FOR GRAB USERSTATE ***
 *
 * This callback is used to fade out the mouse-release-message.
 * The variable msg_cnt is set to 255 when the mouse is grabbed and faded
 * down to zero. For each step the part of the widget which displays the
 * message is redrawn.
 */
#if 0
static void grab_tick_callback(WIDGET *w, int dx, int dy) {
  (void)dx; (void)dy;
	if (msg_cnt > 70) {
		msg_cnt -= 2;
		if (w->vd->fps == 0) redraw->draw_widgetarea(w, msg_x-1, msg_y-1, msg_x+msg_w+1, msg_y+msg_h+1);
	} else if (msg_cnt > 0) {
		msg_cnt = 0;
		if (w->vd->fps == 0) redraw->draw_widgetarea(w, msg_x-1, msg_y-1, msg_x+msg_w+1, msg_y+msg_h+1);
	} else {
		msg_cnt = 0;
	}
}
#endif


/*** HANDLE EVENTS ***
 *
 * We have to take care about the mouse grab mode of the VScreen widget.
 * In grab-mode the mouse is grabbed by any press event onto the widget.
 * When grabbed, the mouse can be discharged by pressing [pause]. All
 * other events go through the normal way.
 */
static void (*orig_handle_event)(WIDGET *w, EVENT *, WIDGET *from);
static void vscr_handle_event(VSCREEN *vs, EVENT *e, WIDGET *from) {
#if 0
	unsigned long m;
	s32 app_id;
	if (e->type == EVENT_PRESS) {
		/* transition from grabbed to grab */
		if (vs->vd->grabmouse == VSCR_MOUSEMODE_GRABBED) {
			if (e->code == L4RE_KEY_PAUSE) {
				vs->vd->grabmouse = VSCR_MOUSEMODE_GRAB;
				m = vs->gen->get_bind_msg(vs, "discharge");
				app_id = vs->gen->get_app_id(vs);
				if (m) msg->send_action_event(app_id, "discharge", m);
				msg_cnt = 0;
				redraw->draw_widgetarea(vs, msg_x, msg_y, msg_x + msg_w, msg_y + msg_h);
				userstate->idle();
				return;
			}
		/* transition to grabbed mouse */
		} else if (vs->vd->grabmouse == VSCR_MOUSEMODE_GRAB) {
			WINDOW *w;
			
			/* top associated window */
			w = (WINDOW *)vs->gen->get_window(vs);
			if (w) w->win->top(w);

			vs->vd->grabmouse = VSCR_MOUSEMODE_GRABBED;
			m = vs->gen->get_bind_msg(vs, "catch");
			app_id = vs->gen->get_app_id(vs);
			if (m) msg->send_action_event(app_id, "catch", m);
			msg_x = (vs->wd->w - msg_w)/2;
			msg_y = (vs->wd->h - msg_h)/2;
			userstate->grab(vs, grab_tick_callback);
			msg_cnt = 255;
			return;
		}
	}
#endif
	/* scale values of motion event */
	if (e->type == L4RE_EV_ABS)
	  {
	    float ratio = 1;
	    if (e->code == L4RE_ABS_X)
	      ratio = (float)vs->vd->xres / (float)vs->wd->w;
	    else if (e->code == L4RE_ABS_Y)
	      ratio = (float)vs->vd->yres / (float)vs->wd->h;

	    e->value = (float)e->value  * ratio;
	  }
	orig_handle_event(vs, e, from);
}


/*** RELEASE VSCREEN DATA ***
 *
 * Eventually, we have to exclude the vscreen from
 * the ring list of vscreens which share one buffer.
 */
static void vscr_free_data(VSCREEN *vs) {

	/* shutdown vscreen server thread */
	if (vs->vd->server_thread)
		thread->kill_thread(vs->vd->server_thread);

	vs->vd->server_thread = NULL;

	/* free the mouse if it is currently grabbed inside the vscreen widget */
	if ((userstate->get() == USERSTATE_GRAB) && (userstate->get_selected() == vs))
		userstate->idle();
	
	if (vs->wd->ref_cnt == 0) vscr_share_exclude(vs);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *vscr_get_type(VSCREEN *vs) {
  (void)vs;
	return "VScreen";
}


/********************************
 *** VSCREEN SPECIFIC METHODS ***
 ********************************/

/*** REGISTER VSCREEN WIDGET SERVER ***/
static void vscr_reg_server(VSCREEN *vs, char *new_server_ident) {
	if (!new_server_ident) return;
	INFO(printf("Vscreen(reg_server): register Vscreen server with ident=%s\n", new_server_ident));
	vs->vd->server_ident = new_server_ident;
}


/*** RETURN VSCREEN WIDGET SERVER ***/
static char *vscr_get_server(VSCREEN *vs) {

	/* allocate thread id for server thread */
	vs->vd->server_thread = thread->alloc_thread();

	/* start widget server */
	if (vscr_server && vs->vd->server_thread)
		vscr_server->start(vs->vd->server_thread, vs);

	INFO(printf("VScreen(get_server): server_ident = %s\n", vs->vd->server_ident));
	return vs->vd->server_ident;
}


/*** SET UPDATE RATE OF THE VSCREEN WIDGET ***/
static void vscr_set_framerate(VSCREEN *vs, s32 framerate) {

	if (framerate == 0) {
		sched->remove(vs);
	} else {
		sched->add(vs, 1000/framerate);
		sched->set_sync_mutex(vs, vs->vd->sync_mutex);
		vs->vd->fps = framerate;
	}
}


/*** REQUEST CURRENT UPDATE RATE ***/
static s32 vscr_get_framerate(VSCREEN *vs) {
	return vs->vd->fps;
}


/*** SET VSCREEN MOUSE X POSITION ***/
static void vscr_set_mx(VSCREEN *vs, s32 new_mx) {
	if (vs->vd->grabmouse != VSCR_MOUSEMODE_GRABBED) return;
	vs_mx = new_mx + vs->gen->get_abs_x(vs);
	vs->vd->update_flags |= VSCR_UPDATE_MOUSE_X;
}


/*** REQUEST VSCREEN MOUSE X POSITION ***/
static s32 vscr_get_mx(VSCREEN *vs) {
	if (vs->vd->grabmouse != VSCR_MOUSEMODE_GRABBED) return 0;
	return vs_mx - vs->gen->get_abs_x(vs);
}


/*** SET VSCREEN MOUSE Y POSITION ***/
static void vscr_set_my(VSCREEN *vs, s32 new_my) {
	if (vs->vd->grabmouse != VSCR_MOUSEMODE_GRABBED) return;
	vs_my = new_my + vs->gen->get_abs_y(vs);
	vs->vd->update_flags |= VSCR_UPDATE_MOUSE_Y;
}


/*** SET WIDTH OF A VSCREEN TO A FIXED VALUE ***/
static void vscr_set_fixw(VSCREEN *vs, char *new_fixw) {
	if (!new_fixw) return;
	if (dope_streq("none", new_fixw, 4)) {
		vs->wd->min_w = 0;
		vs->wd->max_w = 99999;
	} else {
		vs->wd->min_w = vs->wd->max_w = atol(new_fixw);
	}
	vs->wd->update |= WID_UPDATE_MINMAX;
}


/*** SET HEIGHT OF A VSCREEN TO A FIXED VALUE ***/
static void vscr_set_fixh(VSCREEN *vs, char *new_fixh) {
	if (!new_fixh) return;
	if (dope_streq("none", new_fixh, 4)) {
		vs->wd->min_h = 0;
		vs->wd->max_h = 99999;
	} else {
		vs->wd->min_h = vs->wd->max_h = atol(new_fixh);
	}
	vs->wd->update |= WID_UPDATE_MINMAX;
}


/*** REQUEST VSCREEN MOUSE Y POSITION ***/
static s32 vscr_get_my(VSCREEN *vs) {
	if (vs->vd->grabmouse != VSCR_MOUSEMODE_GRABBED) return 0;
	return vs_my - vs->gen->get_abs_y(vs);
}


/*** SET MOUSE GRAB MODE OF THE VSCREEN WIDGET ***/
static void vscr_set_grabmouse(VSCREEN *vs, s32 grab_flag) {
	vs->vd->grabmouse = grab_flag ? VSCR_MOUSEMODE_GRAB : VSCR_MOUSEMODE_FREE;
}


/*** REQUEST MOUSE GRAB MODE ***/
static s32 vscr_get_grabmouse(VSCREEN *vs) {
	if (vs->vd->grabmouse == VSCR_MOUSEMODE_FREE) return 0;
	return 1;
}


/*** TEST IF A GIVEN GRAPHICS MODE IS VALID ***/
static s32 vscr_probe_mode(VSCREEN *vs, s32 width, s32 height, char *mode) {
  (void)vs;
	if ((!dope_streq(mode, "RGB16",  6))
	 && (!dope_streq(mode, "YUV420", 7))
	 && (!dope_streq(mode, "RGBA32", 7))) return 0;
	if (width*height <= 0) return 0;
	return 1;
}


/*** SET GRAPHICS MODE ***/
static s32 vscr_set_mode(VSCREEN *vs, s32 width, s32 height, char *mode) {
	int type = 0;

	if (!vscr_probe_mode(vs, width, height, mode)) return 0;

	/* destroy old image buffer and reset values */
	switch (vs->vd->bpp) {
	case 16:
	case 32:
		if (vs->vd->image) gfx->dec_ref(vs->vd->image);
	}

	vs->vd->bpp     = 0;
	vs->vd->xres    = 0;
	vs->vd->yres    = 0;
	vs->vd->pixels  = NULL;
	vs->vd->image   = NULL;
	vs->vd->vw      = vs->vd->vh      = 0;
	vs->vd->curr_vx = vs->vd->next_vx = 0;
	vs->vd->curr_vy = vs->vd->next_vy = 0;

	/* create new frame buffer image */
	if (dope_streq("RGB16",  mode, 6)) type = GFX_IMG_TYPE_RGB16;
	if (dope_streq("RGBA32", mode, 7)) type = GFX_IMG_TYPE_RGBA32;
	if (dope_streq("YUV420", mode, 7)) type = GFX_IMG_TYPE_YUV420;

	if (!type) {
		ERROR(printf("VScreen(set_mode): mode %s not supported!\n", mode);)
		return 0;
	}

	if ((vs->vd->image = gfx->alloc_img(width, height, type))) {
		vs->vd->xres   = width;
		vs->vd->yres   = height;
		vs->vd->vw     = width;
		vs->vd->vh     = height;
		vs->vd->pixels = gfx->map(vs->vd->image);
		gfx->get_ident(vs->vd->image, &vs->vd->smb_ident[0]);
	} else {
		ERROR(printf("VScreen(set_mode): out of memory!\n");)
		return 0;
	}

	if (type == GFX_IMG_TYPE_RGBA32)
		vs->wd->flags &= ~WID_FLAGS_CONCEALING;
	else
		vs->wd->flags |= WID_FLAGS_CONCEALING;

	vs->gen->update(vs);
	return 1;
}


/*** WAIT FOR THE NEXT SYNC ***/
static void vscr_waitsync(VSCREEN *vs) {
	thread->mutex_down(vs->vd->sync_mutex);
}


/*** UPDATE A SPECIFIED AREA OF THE WIDGET ***/
static void vscr_refresh(VSCREEN *vs, s32 x, s32 y, s32 w, s32 h) {
	VSCREEN *last = vs;
	int sx1, sy1, sx2, sy2;
	float mx, my;

	int cnt = 1;
	if (vs->vd->share_next) {
		while ((vs = vs->vd->share_next) != last) cnt++;
	}

	if (w == -1) w = vs->vd->xres;
	if (h == -1) h = vs->vd->yres;
	if ((w<1) || (h<1)) return;
	
	/* refresh all vscreens which share the same pixel buffer */
	while (cnt--) {
		mx = (float)vs->wd->w / (float)vs->vd->xres;
		my = (float)vs->wd->h / (float)vs->vd->yres;
		sx1 = x*mx;
		sy1 = y*my;
		sx2 = (int)((x + w)*mx);
		sy2 = (int)((y + h)*my);
		redraw->draw_widgetarea(vs, sx1, sy1, sx2, sy2);
		vs = vs->vd->share_next;
	}
}


/*** MAP VSCREEN BUFFER TO ANOTHER THREAD'S ADDRESS SPACE ***/
static char *vscr_map(VSCREEN *vs, char *dst_thread_ident) {
  (void)dst_thread_ident;
	s32 app_id;
	char dst_th_buf[16];
	THREAD *dst_th = (THREAD *)(void *)dst_th_buf;
	
	if (!vs->vd->image) return "Error: VScreen mode not initialized.";

	/* if no valid thread identifier was suppied we map to the app's thread */
	/*if (thread->ident2thread(dst_thread_ident, dst_th))*/ {
		app_id = vs->gen->get_app_id(vs);
		dst_th = appman->get_app_thread(app_id);
	}
	gfx->share(vs->vd->image, dst_th);
	INFO(printf("VScreen(map): return vs->vd->smb_ident = %s\n", &vs->vd->smb_ident[0]));
	return &vs->vd->smb_ident[0];
}


/*** SHARE IMAGE BUFFER WITH OTHER SPECIFIED VSCREEN ***/
static void vscr_share(VSCREEN *vs, VSCREEN *from) {
	int img_type = 0;
	GFX_CONTAINER *new_image;

	/* exclude widget from its previous ring list of buffer-shared vscreen */
	if (vs->vd->share_next) vscr_share_exclude(vs);

	if (!from || !(new_image = from->vscr->get_image(from))) return;

	/*
	 * Increment reference counter of new image before decrementing
	 * the reference counter of the old one. If both images are the
	 * same, we do not want to risk a reference counter of zero.
	 */
	gfx->inc_ref(new_image);

	/* replace old image with new one */
	if (vs->vd->image) gfx->dec_ref(vs->vd->image);
	vs->vd->image = new_image;

	vs->vd->vw = vs->vd->xres = gfx->get_width(vs->vd->image);
	vs->vd->vh = vs->vd->yres = gfx->get_height(vs->vd->image);
	img_type = gfx->get_type(vs->vd->image);
	switch (img_type) {
	case GFX_IMG_TYPE_RGB16:
		vs->vd->bpp = 16;
		break;
	case GFX_IMG_TYPE_RGBA32:
		vs->vd->bpp = 32;
		break;
	case GFX_IMG_TYPE_YUV420:
		vs->vd->bpp = 12;
		break;
	}
	vs->vd->pixels = gfx->map(vs->vd->image);

	vs->gen->set_w(vs, vs->vd->xres);
	vs->gen->set_h(vs, vs->vd->yres);
	vs->gen->updatepos(vs);

	/* join the new ring list of buffer sharing vscreens */
	vscr_share_join(from, vs);
}


static GFX_CONTAINER *vscr_get_image(VSCREEN *vs) {
	return vs->vd->image;
}


static struct widget_methods gen_methods;
static struct vscreen_methods vscreen_methods = {
	vscr_reg_server,
	vscr_waitsync,
	vscr_refresh,
	vscr_get_image,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static VSCREEN *create(void) {

	VSCREEN *new = ALLOC_WIDGET(struct vscreen);
	SET_WIDGET_DEFAULTS(new, struct vscreen, &vscreen_methods);

	/* set widget type specific data */
	new->vd->sync_mutex = thread->create_mutex(1);  /* locked */
	new->wd->flags |= WID_FLAGS_CONCEALING | WID_FLAGS_EDITABLE;
	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct vscreen_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("VScreen", (void *(*)(void))create);

	script->reg_widget_method(widtype, "long probemode(long width, long height, string mode)", vscr_probe_mode);
	script->reg_widget_method(widtype, "long setmode(long width, long height, string mode)", vscr_set_mode);
	script->reg_widget_method(widtype, "string getserver()", vscr_get_server);
	script->reg_widget_method(widtype, "string map(string thread=\"caller\")", vscr_map);
	script->reg_widget_method(widtype, "void refresh(long x=0, long y=0, long w=-1, long h=-1)", vscr_refresh);
	script->reg_widget_method(widtype, "void share(Widget from)", vscr_share);

	script->reg_widget_attrib(widtype, "long framerate", vscr_get_framerate, vscr_set_framerate, gen_methods.update);
	script->reg_widget_attrib(widtype, "string fixw", NULL, vscr_set_fixw, gen_methods.update);
	script->reg_widget_attrib(widtype, "string fixh", NULL, vscr_set_fixh, gen_methods.update);
	script->reg_widget_attrib(widtype, "long mousex", vscr_get_mx, vscr_set_mx, gen_methods.update);
	script->reg_widget_attrib(widtype, "long mousey", vscr_get_my, vscr_set_my, gen_methods.update);
	script->reg_widget_attrib(widtype, "boolean grabmouse", vscr_get_grabmouse, vscr_set_grabmouse, gen_methods.update);
	widman->build_script_lang(widtype, &gen_methods);
}


int init_vscreen(struct dope_services *d) {

	gfx         = d->get_module("Gfx 1.0");
	script      = d->get_module("Script 1.0");
	widman      = d->get_module("WidgetManager 1.0");
	redraw      = d->get_module("RedrawManager 1.0");
	appman      = d->get_module("ApplicationManager 1.0");
	thread      = d->get_module("Thread 1.0");
	sched       = d->get_module("Scheduler 1.0");
	vscr_server = d->get_module("VScreenServer 1.0");
	msg         = d->get_module("Messenger 1.0");
	userstate   = d->get_module("UserState 1.0");
	font        = d->get_module("FontManager 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_update          = gen_methods.update;
	orig_handle_event    = gen_methods.handle_event;
	
	gen_methods.get_type     = vscr_get_type;
	gen_methods.draw         = vscr_draw;
	gen_methods.update       = vscr_update;
	gen_methods.free_data    = vscr_free_data;
	gen_methods.handle_event = vscr_handle_event;

	build_script_lang();

	msg_fid = 0;
	msg_txt = "press [pause] to release mouse";
	msg_w = font->calc_str_width(msg_fid, msg_txt);
	msg_h = font->calc_str_height(msg_fid, msg_txt);

	d->register_module("VScreen 1.0", &services);
	return 1;
}
