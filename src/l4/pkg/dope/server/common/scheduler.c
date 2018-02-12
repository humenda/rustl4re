/*
 * \brief   DOpE simple scheduling module
 * \date    2004-04-27
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * There are four time-slots where real-time opera-
 * tions can be  (interleaved) executed.  Given an
 * execution frequency  of 100Hz - this rt-manager
 * module displays the rt-widgets at 25fps.
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
#include "widget.h"
#include "widget_data.h"
#include "thread.h"
#include "window.h"
#include "screen.h"
#include "userstate.h"
#include "redraw.h"
#include "timer.h"
#include "scheduler.h"
#include "grid.h"
#include "loaddisplay.h"

static struct thread_services      *thread;
static struct timer_services       *timer;
static struct grid_services        *grid;
static struct window_services      *win;
static struct redraw_services      *redraw;
static struct userstate_services   *userstate;
static struct screen_services      *screen;
static struct loaddisplay_services *loaddisplay;

#define NUM_SLOTS 4
struct timeslot {
	WIDGET *w;
	MUTEX  *sync_mutex;
} ts[NUM_SLOTS];

extern SCREEN *curr_scr;

int init_simple_scheduler(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** REGISTER NEW REAL-TIME WIDGET ***
 *
 * The period argument is ignored as we only support 40ms.
 */
static s32 rt_add_widget(WIDGET *w, u32 period) {
  (void)period;
	s32 free_slot = -1;
	s32 i;

	/* search free slot */
	for (i=0;i<NUM_SLOTS;i++) {
		if (!ts[i].w) free_slot = i;
	}

	if (free_slot == -1) return -1;

	/* settle down at time slot */
	ts[free_slot].w = w;
	w->gen->inc_ref(w);

	return 0;
}


/*** UNREGISTER A REAL-TIME WIDGET ***/
static void rt_remove_widget(WIDGET *w) {
	s32 i;

	/* search slot of the given widget */
	for (i=0;i<NUM_SLOTS;i++) {

		/* free the widget's time slot */
		if (ts[i].w == w) {
			ts[i].w = NULL;
			ts[i].sync_mutex = NULL;
			w->gen->dec_ref(w);
			return;
		}
	}
}


/*** UNREGISTER ALL REAL-TIME WIDGETS OF SPECIFIED APPLICATION ***/
static void rt_release_app(int app_id) {
	s32 i;
	for (i=0;i<NUM_SLOTS;i++) {
		WIDGET *w = ts[i].w;
		if (w && (w->gen->get_app_id(w) == app_id))
			rt_remove_widget(w);
	}
}


/*** SET MUTEX THAT SHOULD BE UNLOCKED AFTER DRAWING OPERATIONS ***/
static void rt_set_sync_mutex(WIDGET *w,MUTEX *m) {
	s32 i;

	/* search slot of the given widget */
	for (i=0;i<NUM_SLOTS;i++) {
		if (ts[i].w == w) {
			ts[i].sync_mutex = m;
			return;
		}
	}
}


/*** MAINLOOP OF DOpE ***
 *
 * Within the mainloop we must do the following things:
 *
 * * Perform the redraw of real-time and non-real-time widgets.
 * * Call the userstate manager periodically.
 */
static void process_mainloop(void) {
	GRID *g;
	LOADDISPLAY *ld[4];
	WIDGET *cw;
	s32 i,j;
	u32 start_time, rt_end_time, end_time, usr_end_time;
	u32 curr_length;
	s32 period_clock  = 10000;
	s32 period_length = 8000;

	s32 slot_usr_time[4];
	s32 slot_rt_time[4];
	s32 slot_nrt_time[4];
	s32 curr_slot = 0;

	/*** create slot display window ***/
	WINDOW *w1 = win->create();
	g = grid->create();
	for (i=0; i<4; i++) {
		ld[i] = loaddisplay->create();
		ld[i]->ldm->set_orient(ld[i], "vertical");
		ld[i]->ldm->set_from(ld[i], 100.0);
		ld[i]->ldm->set_to(ld[i], 0.0);
		ld[i]->gen->update((WIDGET *)ld[i]);
		ld[i]->ldm->barconfig(ld[i], "user", "0", "green");
		ld[i]->ldm->barconfig(ld[i], "rt",   "0", "red"  );
		ld[i]->ldm->barconfig(ld[i], "nrt",  "0", "blue" );

		g->grid->add(g, (WIDGET *)ld[i]);
		g->grid->set_row(g, (WIDGET *)ld[i], 1);
		g->grid->set_col(g, (WIDGET *)ld[i], i);
	}
	g->gen->update((WIDGET *)g);
	w1->win->set_content(w1,(WIDGET *)g);
	w1->gen->update((WIDGET *)w1);
//	curr_scr->scr->place(curr_scr, (WIDGET *)w1, 10, 110, 74, 250);

	redraw->exec_redraw(100*1000*1000);

	/*** entering mainloop ***/
	INFO(printf("starting eventloop\n"));

	while (1) {

		/*** pay some attentation to the user ***/
		start_time = timer->get_time();
		curr_scr->gen->lock((WIDGET *)curr_scr);
		userstate->handle();
		curr_scr->gen->unlock((WIDGET *)curr_scr);
		usr_end_time = timer->get_time();

		/*** process real-time widgets ***/
		
		/* cycle trough time slots */
		curr_slot = (curr_slot + 1) % NUM_SLOTS;

		if ((cw = ts[curr_slot].w)) {
			cw->gen->lock(cw);
			cw->gen->drawarea(cw, cw, 0, 0, cw->wd->w, cw->wd->h);
			cw->gen->unlock(cw);
		}

		if (ts[curr_slot].sync_mutex) thread->mutex_up(ts[curr_slot].sync_mutex);

		rt_end_time = timer->get_time();

		/*** process non-real-time widgets ***/

		// s32 left_time = (s32)period_length - (s32)timer->get_diff(start_time,timer->get_time());
		redraw->exec_redraw_all();
//		redraw->exec_redraw(left_time);
		end_time = timer->get_time();

		curr_length = timer->get_diff(start_time,end_time);

//		timer->usleep(10*1000);
		if ((s32)curr_length < period_length)
			timer->usleep(period_clock - curr_length);

//		timer->usleep(1000*30);
		
		slot_usr_time[curr_slot] += usr_end_time - start_time;
		slot_rt_time[curr_slot]  += rt_end_time - usr_end_time;
		slot_nrt_time[curr_slot] += end_time - rt_end_time;

		/*** update load bars ***/

//		i--;
		if (i<=0) {
			static char buf[32];
			i += 20;
			for (j=0; j<4; j++) {
				snprintf(buf, 32, "%d", (int)(slot_usr_time[j])/((period_length*5)/100));
				ld[j]->ldm->barconfig(ld[j], "user", buf, "<default>");
				snprintf(buf, 32, "%d", (int)(slot_rt_time[j])/((period_length*5)/100));
				ld[j]->ldm->barconfig(ld[j], "rt",   buf, "<default>");
				snprintf(buf, 32, "%d", (int)(slot_nrt_time[j])/((period_length*5)/100));
				ld[j]->ldm->barconfig(ld[j], "nrt",  buf, "<default>");
				slot_rt_time[j] = 0;
				slot_nrt_time[j] = 0;
				slot_usr_time[j] = 0;
			}
		}
	}
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scheduler_services services = {
	rt_add_widget,
	rt_remove_widget,
	rt_release_app,
	rt_set_sync_mutex,
	process_mainloop,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_simple_scheduler(struct dope_services *d) {

	thread      = d->get_module("Thread 1.0");
	win         = d->get_module("Window 1.0");
	userstate   = d->get_module("UserState 1.0");
	redraw      = d->get_module("RedrawManager 1.0");
	timer       = d->get_module("Timer 1.0");
	screen      = d->get_module("Screen 1.0");
	grid        = d->get_module("Grid 1.0");
	loaddisplay = d->get_module("LoadDisplay 1.0");

	d->register_module("Scheduler 1.0",&services);
	return 1;
}
