/*
 * \brief   DOpE user state module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This component manages the different states of,
 * the user interface. These states depend on the
 * users action.
 *
 * idle       - user drinks coffee
 * drag       - user drags a widget with the mouse
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "event.h"
#include "widget.h"
#include "input.h"
#include "scrdrv.h"
#include "screen.h"
#include "userstate.h"
#include "redraw.h"
#include "tick.h"
#include "messenger.h"
#include "widget_data.h"
#include "keymap.h"
#include "keycodes.h"
#include "window.h"

#include <l4/input/macros.h>
#include <l4/re/env.h>

static struct input_services  *input;
static struct scrdrv_services *scrdrv;
static struct redraw_services *redraw;
static struct tick_services   *tick;
static struct keymap_services *keymap;

static s32     omx,omy,omb;                 /* original mouse postion     */
static s32     curr_mx, curr_my;            /* current mouse position     */
static s32     max_mx = 200, max_my = 200;  /* max mouse position         */
static s32     curr_mb;                     /* current mouse button state */
static s32     curr_state = USERSTATE_IDLE; /* current user state         */
static WIDGET *curr_selected;               /* currently selected widget  */
static WIDGET *curr_mfocus;                 /* current mouse focus        */
static WINDOW *curr_window;                 /* currently active window    */
static WIDGET *curr_receiver;               /* current receiver of motion */
static void  (*curr_motion_callback)  (WIDGET *,int,int);
static void  (*curr_release_callback) (WIDGET *,int,int);
static void  (*curr_tick_callback)    (WIDGET *,int,int);
static int     press_cnt;                   /* number of pressed keys     */
static char    keytab[DOPE_KEY_MAX];
static s32     curr_keystate;               /* current key state          */
static long    curr_keycode;                /* code of curr. pressed key  */
static s32     key_repeat_delay = 250;      /* delay until key repeat     */
static s32     key_repeat_rate = 30;        /* key repeat rate            */

#define USERSTATE_KEY_IDLE   0x0            /* no key pressed             */
#define USERSTATE_KEY_PRESS  0x1            /* key pressed                */
#define USERSTATE_KEY_REPEAT 0x2            /* long press - key repeat    */

extern SCREEN *curr_scr;

int init_userstate(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** RETURN WHETER A KEYCODES SETS THE KEYBOARD FOCUS ***
 *
 * Normally, the keyboard focus can be defined via the
 * mouse buttons while plain keys will be forwarded to
 * the currently active keyboard focused widget
 */
static inline int key_sets_focus(long keycode) {
	return (keycode >= BTN_LEFT && keycode <= BTN_MIDDLE);
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** DETERMINE ASCII VALUE OF A KEY WHILE TAKING MODIFIER KEYS INTO ACCOUNT ***/
static char get_ascii(long keycode) {
	long switches=0;
	if (keycode>=DOPE_KEY_MAX) return 0;
	if (keytab[42] ) switches = switches | KEYMAP_SWITCH_LSHIFT;
	if (keytab[54] ) switches = switches | KEYMAP_SWITCH_RSHIFT;
	if (keytab[29] ) switches = switches | KEYMAP_SWITCH_LCONTROL;
	if (keytab[97] ) switches = switches | KEYMAP_SWITCH_RCONTROL;
	if (keytab[56] ) switches = switches | KEYMAP_SWITCH_ALT;
	if (keytab[100]) switches = switches | KEYMAP_SWITCH_ALTGR;
	return keymap->get_ascii(keycode,switches);
}


/*** FORCE A NEW MOUSE POSITION ***/
static void set_pos(long mx, long my) {
	if (mx < 0) mx = 0;
	if (my < 0) my = 0;
	if (mx > max_mx) mx = max_mx;
	if (my > max_my) my = max_my;
	curr_mx = mx;
	curr_my = my;
}


/*** MAKE SPECIFIED WINDOW THE ACTIVE ONE ***
 *
 * If the force argument is zero, the window is activated only
 * if the old active window belongs to the same application.
 * This way, we prevent applications from stealing the keyboard
 * focus from other applications.
 */
static void set_active_window(WINDOW *w, int force) {
	if (w == curr_window) return;
	if (!force && curr_window && w && curr_window->wd->app_id != w->wd->app_id) return;

	if (w) w->win->activate(w);

	if (curr_window) curr_window->gen->dec_ref((WIDGET *)curr_window);
	curr_window = w;
	if (curr_window) curr_window->gen->inc_ref((WIDGET *)curr_window);
}


/*** RELEASE CURRENT USERSTATE ***/
static void leave_current(void) {

	switch (curr_state) {
	case USERSTATE_DRAG:
		if (curr_release_callback) {
			curr_release_callback(curr_selected, curr_mx - omx, curr_my - omy);
		}
		break;
	case USERSTATE_TOUCH:
		if (curr_release_callback) {
			curr_release_callback(curr_selected, curr_mx - omx, curr_my - omy);
		}
		curr_selected->gen->set_state(curr_selected, 0);
		if (curr_mfocus != curr_selected) {
			curr_selected->gen->set_mfocus(curr_selected,0);
		}
		curr_selected->gen->update(curr_selected);
		break;
	case USERSTATE_GRAB:
		set_pos(omx, omy);
		scrdrv->set_mouse_pos(omx,omy);
		break;
	default:
		break;
	}
}


/*** SET NEW USER STATE ***/
static void idle(void) {
	leave_current();
	curr_state = USERSTATE_IDLE;
}


/*** ENTER TOUCH-USERSTATE ***/
static void touch(WIDGET *w, void (*tick_callback)   (WIDGET *,int dx, int dy),
                             void (*release_callback)(WIDGET *,int dx, int dy)) {
	leave_current();
	if (w) w->gen->inc_ref(w);
	if (curr_selected) curr_selected->gen->dec_ref(curr_selected);
	curr_selected = w;
	omx = curr_mx;
	omy = curr_my;
	omb = curr_mb;
	curr_tick_callback    = tick_callback;
	curr_release_callback = release_callback;
	curr_state = USERSTATE_TOUCH;
	curr_selected->gen->set_state(curr_selected, 1);
	curr_selected->gen->update(curr_selected);
}


/*** ENTER DRAG-USERSTATE ***/
static void drag(WIDGET *w, void (*motion_callback) (WIDGET *,int dx, int dy),
                            void (*tick_callback)   (WIDGET *,int dx, int dy),
                            void (*release_callback)(WIDGET *,int dx, int dy)) {
	leave_current();
	if (w) w->gen->inc_ref(w);
	if (curr_selected) curr_selected->gen->dec_ref(curr_selected);
	curr_selected = w;
	omx = curr_mx;
	omy = curr_my;
	omb = curr_mb;
	curr_motion_callback  = motion_callback;
	curr_tick_callback    = tick_callback;
	curr_release_callback = release_callback;
	curr_state = USERSTATE_DRAG;
}


/*** ENTER MOUSE GRAB USERSTATE ***/
static void grab(WIDGET *w, void (*tick_callback) (WIDGET *, int, int)) {
	leave_current();
	omx = curr_mx;
	omy = curr_my;
	scrdrv->set_mouse_pos(5000,5000);
	curr_tick_callback = tick_callback;
	if (w) w->gen->inc_ref(w);
	if (curr_selected) curr_selected->gen->dec_ref(curr_selected);
	curr_selected = w;
	curr_state = USERSTATE_GRAB;
}


static long get(void) {
	return curr_state;
}


/*** TICK CALLBACK THAT IS CALLED FOR EVERY REPEATED KEY ***/
static int tick_handle_repeat(void *arg) {
	long keycode = (long)arg;
	EVENT key_repeat_event;
	if (curr_keystate != USERSTATE_KEY_REPEAT || curr_keycode != keycode)
		return 0;

	key_repeat_event.time = l4_kip_clock(l4re_kip());
	key_repeat_event.type = EV_KEY;
	key_repeat_event.code = curr_keycode;
	key_repeat_event.value = 2; // repeat

	if (curr_window) {
		WIDGET *cw = curr_window->win->get_kfocus(curr_window);
		if (cw) cw->gen->handle_event(cw, &key_repeat_event, NULL);
	}
	return 1;
}


/*** TICK CALLBACK THAT IS CALLED AFTER KEY DELAY ***/
static int tick_handle_delay(void *arg) {
	long keycode = (long)arg;
	if (curr_keystate != USERSTATE_KEY_PRESS || curr_keycode != keycode)
		return 0;

	curr_keystate = USERSTATE_KEY_REPEAT;
	tick->add(key_repeat_rate, tick_handle_repeat, arg);
	return 0;
}


/*** UPDATE MOUSE FOCUS ***/
static void update_mfocus(void) {
	static EVENT event;

	WIDGET *new_mfocus = NULL;

	if (press_cnt != 0) return;

	/* FIXME: scr->find must be added here! */
	new_mfocus = curr_scr->gen->find((WIDGET *)curr_scr, curr_mx, curr_my);

	if (new_mfocus == curr_mfocus) return;

	if (curr_mfocus) {
		curr_mfocus->gen->set_mfocus(curr_mfocus, 0);
		if (curr_mfocus->wd->flags & WID_FLAGS_HIGHLIGHT)
			curr_mfocus->gen->update(curr_mfocus);
		event.time = l4_kip_clock(l4re_kip());
		event.type = EV_WINDOW;
		event.value = 0;
		curr_mfocus->gen->handle_event(curr_mfocus, &event, NULL);
		curr_mfocus->gen->dec_ref(curr_mfocus);
	}
	if (new_mfocus) {
		new_mfocus->gen->set_mfocus(new_mfocus, 1);
		if (new_mfocus->wd->flags & WID_FLAGS_HIGHLIGHT)
			new_mfocus->gen->update(new_mfocus);
		event.time = l4_kip_clock(l4re_kip());
		event.type = EV_WINDOW;
		event.value = 1;
		new_mfocus->gen->handle_event(new_mfocus, &event, NULL);
		new_mfocus->gen->inc_ref(new_mfocus);
	}
	curr_mfocus = new_mfocus;
}


static void handle(void)
{
  static long    old_mx, old_my;
  long    update_needed = 0;
  EVENT   event;

  WIDGET *new_mfocus = NULL;

  old_mx = curr_mx;
  old_my = curr_my;

  while (input->get_event(&event))
    {
      switch (event.type)
	{
	case EV_REL:
	  if (event.code == REL_X)
	    set_pos(curr_mx + event.value, curr_my);
	  else if (event.code == REL_Y)
	    set_pos(curr_mx, curr_my + event.value);
	  break;

	case EV_ABS:
	  if (event.code == ABS_X)
	    set_pos(event.value, curr_my);
	  else if (event.code == ABS_Y)
	    set_pos(curr_mx, event.value);
	  break;

	case EV_KEY:
	  if (event.value == 1)
	    {
	      press_cnt++;

	      if (event.code == BTN_LEFT)  curr_mb = curr_mb | 0x01;
	      if (event.code == BTN_RIGHT) curr_mb = curr_mb | 0x02;
	      keytab[event.code] = 1;
	      if (get_ascii(event.code)
		  || (event.code >= KEY_UP && event.code <= KEY_DELETE))
		{
		  curr_keystate = USERSTATE_KEY_PRESS;
		  curr_keycode  = event.code;
		  tick->add(key_repeat_delay, tick_handle_delay, (void *)curr_keycode);
		}
	      else
		{
		  curr_keystate = USERSTATE_KEY_IDLE;
		  curr_keycode  = 0;
		}
	    }
	  else if (event.value == 0)
	    {
	      press_cnt--;

	      if (event.code == BTN_LEFT)  curr_mb = curr_mb & 0x00fe;
	      if (event.code == BTN_RIGHT) curr_mb = curr_mb & 0x00fd;
	      keytab[event.code] = 0;
	      curr_keystate = USERSTATE_KEY_IDLE;
	      curr_keycode  = 0;
	    }
	  break;
	}

      update_mfocus();

      if (event.type == EV_KEY)
	{

	  WIDGET *win_kfocus = NULL;

	  /* make clicked window the active one */
	  if (curr_mfocus && key_sets_focus(event.code))
	    {
	      WINDOW *w = (WINDOW *)curr_mfocus->gen->get_window(curr_mfocus);
	      set_active_window(w, 1);
	    }

	  if (curr_window)
	    {
	      win_kfocus = curr_window->win->get_kfocus(curr_window);

	      /* redefine keyboard focus */
	      if (curr_mfocus && key_sets_focus(event.code) && curr_mfocus != win_kfocus)
		curr_mfocus->gen->focus(curr_mfocus);

	      /* request new keyboard focus - just in case it denied the focus */
	      win_kfocus = curr_window->win->get_kfocus(curr_window);
	    }

	  if ((event.value == 1) && (press_cnt == 1))
	    {

	      WIDGET *old_receiver = curr_receiver;

	      /* send keyboard event to actually focused widget if set */
	      if (win_kfocus && !key_sets_focus(event.code))
		{
		  if (win_kfocus) win_kfocus->gen->inc_ref(win_kfocus);
		  curr_receiver = win_kfocus;
		}
	      else
		{
		  if (curr_mfocus) curr_mfocus->gen->inc_ref(curr_mfocus);
		  curr_receiver = curr_mfocus;
		}

	      if (old_receiver)
		old_receiver->gen->dec_ref(old_receiver);
	    }

	  if (curr_receiver)
	    curr_receiver->gen->handle_event(curr_receiver, &event, NULL);
      }
  }

  /*
   * Hell! We got more key release events than press events
   * This can happen when a key is pressed during the bootup
   */
  if (press_cnt < 0) press_cnt = 0;

  tick->handle();

  switch (curr_state)
    {

    case USERSTATE_IDLE:

      /* if mouse position changed -> deliver motion event */
      if (old_mx != curr_mx && curr_mfocus)
	{
	  event.type  = EV_ABS;
	  event.code  = ABS_X;
	  event.value  = curr_mx - curr_mfocus->gen->get_abs_x(curr_mfocus);
	  curr_mfocus->gen->handle_event(curr_mfocus, &event, NULL);
	}
      if (old_my != curr_my && curr_mfocus)
	{
	  event.type  = EV_ABS;
	  event.code  = ABS_Y;
	  event.value  = curr_my - curr_mfocus->gen->get_abs_y(curr_mfocus);
	  curr_mfocus->gen->handle_event(curr_mfocus, &event, NULL);
	}
      break;

    case USERSTATE_TOUCH:

      if (curr_tick_callback)
	curr_tick_callback(curr_selected, curr_mx - omx, curr_my - omy);

      if (press_cnt == 0)
	idle();

      new_mfocus = curr_scr->gen->find((WIDGET *)curr_scr, curr_mx, curr_my);
      if (new_mfocus != curr_mfocus)
	{
	  if (new_mfocus == curr_selected)
	    {
	      curr_selected->gen->set_state(curr_selected,1);
	      curr_selected->gen->update(curr_selected);
	    }
	  else
	    {
	      curr_selected->gen->set_state(curr_selected,0);
	      curr_selected->gen->update(curr_selected);
	    }
	  if (curr_mfocus)
	    curr_mfocus->gen->dec_ref(curr_mfocus);

	  curr_mfocus = new_mfocus;

	  if (curr_mfocus)
	    curr_mfocus->gen->inc_ref(curr_mfocus);
	}
      break;

    case USERSTATE_DRAG:

      if (press_cnt == 0)
	idle();

      if (curr_tick_callback)
	{
	  curr_tick_callback(curr_selected, curr_mx - omx, curr_my - omy);
	}
      if (old_mx != curr_mx || old_my != curr_my || update_needed)
	{
	  update_needed = 1;
	  if (curr_motion_callback && !redraw->is_queued((WIDGET *)curr_scr))
	    {
	      update_needed = 0;
	      curr_motion_callback(curr_selected, curr_mx - omx, curr_my - omy);
	    }
	}
      break;

    case USERSTATE_GRAB:
#if 0
      /* if mouse position changed -> deliver motion event */
      if (old_mx != curr_mx || old_my != curr_my)
	{
	  s32 min_x = curr_selected->gen->get_abs_x(curr_selected);
	  s32 min_y = curr_selected->gen->get_abs_y(curr_selected);
	  s32 max_x = min_x + curr_selected->gen->get_w(curr_selected) - 1;
	  s32 max_y = min_y + curr_selected->gen->get_h(curr_selected) - 1;

	  if (curr_mx < min_x || curr_my < min_y ||
	      curr_mx > max_x || curr_my > max_y)
	    {
	      if (curr_mx < min_x) curr_mx = min_x;
	      if (curr_my < min_y) curr_my = min_y;
	      if (curr_mx > max_x) curr_mx = max_x;
	      if (curr_my > max_y) curr_my = max_y;
	      set_pos(curr_mx, curr_my);
	    }

	  event.type  = EVENT_MOTION;
	  event.abs_x = curr_mx - min_x;
	  event.abs_y = curr_my - min_y;
	  event.rel_x = curr_mx - old_mx;
	  event.rel_y = curr_my - old_my;
	  curr_selected->gen->handle_event(curr_selected, &event, NULL);
	}
      if (curr_tick_callback)
	{
	  curr_tick_callback(curr_selected, curr_mx - omx, curr_my - omy);
	}
#endif
      break;
    }
  scrdrv->set_mouse_pos(curr_mx, curr_my);
}


static WIDGET *get_curr_mfocus(void) {
	return curr_mfocus;
}


static WIDGET *get_curr_selected(void) {
	return curr_selected;
}


/*** REQUEST MOUSE STATE ***/
static long get_mx(void) {return curr_mx;}
static long get_my(void) {return curr_my;}
static long get_mb(void) {return curr_mb;}


/*** SET MAX X POSITION OF MOUSE CURSOR ***/
static void set_max_mx(long new_max_mx) {
	if (curr_mx > new_max_mx) curr_mx = new_max_mx;
	max_mx = new_max_mx;
}


/*** SET MAX Y POSITION OF MOUSE CURSOR ***/
static void set_max_my(long new_max_my) {
	if (curr_my > new_max_my) curr_my = new_max_my;
	max_my = new_max_my;
}


/*** GET CURRENT STATE OF THE KEY WITH THE SPECIFIED KEYCODE ***/
static long get_keystate(long keycode) {
	if (keycode>=DOPE_KEY_MAX) return 0;
	return keytab[keycode];
}


/*** RELEASE REFERENCES TO WIDGETS OF SPECIFIED APPLICATION ***
 *
 * When an application exists, we need to prevent further references to
 * widgets of the application, for example the mouse focus. Therefore,
 * this function should be called on application exit.
 */
static void release_app(int app_id) {

	if (curr_selected && (curr_selected->gen->get_app_id(curr_selected) == app_id))
		idle();

	if (curr_window && (curr_window->gen->get_app_id((WIDGET *)curr_window) == app_id))
		set_active_window(NULL, 1);

	if (curr_mfocus && (curr_mfocus->gen->get_app_id(curr_mfocus) == app_id)) {
		curr_mfocus->gen->dec_ref(curr_mfocus);
		curr_mfocus = NULL;
	}

	if (curr_receiver && (curr_receiver->gen->get_app_id(curr_receiver) == app_id)) {
		curr_receiver->gen->dec_ref(curr_receiver);
		curr_receiver = NULL;
	}

	curr_receiver = NULL;
	curr_selected = NULL;
	curr_window   = NULL;
	curr_mfocus   = NULL;
}


/*** RELEASE WIDGET FROM USERSTATE MANAGER ***
 *
 * When we destroy or relocate widgets, we need to flush its
 * old relationship with the userstate manager.
 */
static void release_widget(WIDGET *w) {
	WIDGET *cw;

	/* check if widget has the current mouse focus as child */
	if (curr_mfocus && w->gen->related_to(w, curr_mfocus)) {
		cw = curr_mfocus;
		curr_mfocus = NULL;
		cw->gen->dec_ref(cw);
	}

	/* check if widget has the current mouse focus as child */
	if (curr_receiver && w->gen->related_to(w, curr_receiver)) {
		cw = curr_receiver;
		curr_receiver = NULL;
		cw->gen->dec_ref(cw);
	}
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct userstate_services services = {
	idle,
	touch,
	drag,
	grab,
	get,
	handle,
	get_curr_mfocus,
	set_active_window,
	get_curr_selected,
	get_mx,
	get_my,
	get_mb,
	set_pos,
	get_keystate,
	get_ascii,
	set_max_mx,
	set_max_my,
	release_app,
	release_widget,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_userstate(struct dope_services *d) {

	input   = d->get_module("Input 1.0");
	scrdrv  = d->get_module("ScreenDriver 1.0");
	redraw  = d->get_module("RedrawManager 1.0");
	tick    = d->get_module("Tick 1.0");
	keymap  = d->get_module("Keymap 1.0");

	d->register_module("UserState 1.0",&services);
	return 1;
}
