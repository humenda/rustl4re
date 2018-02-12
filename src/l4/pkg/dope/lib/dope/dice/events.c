/*
 * \brief   Event handling of DOpE client library
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopeapp-server.h"
#include "dopeapp-client.h"
#include "dopestd.h"
#include "dopelib.h"
#include "events.h"
#include "sync.h"
#include "app_struct.h"
#include "misc.h"


/*** UTILITIY: CONVERT DOPE EVENT TO IDL TYPE ***/
static void dope_event__to__dope_event_u(dope_event *ev, dope_event_u *ev_u) {

	switch (ev->type) {

		case EVENT_TYPE_COMMAND:
			ev_u->type = 1;
			ev_u->_u.command.cmd = ev->command.cmd;
			break;

		case EVENT_TYPE_MOTION:
			ev_u->type = 2;
			ev_u->_u.motion.rel_x = ev->motion.rel_x;
			ev_u->_u.motion.rel_y = ev->motion.rel_y;
			ev_u->_u.motion.abs_x = ev->motion.abs_x;
			ev_u->_u.motion.abs_y = ev->motion.abs_y;
			break;

		case EVENT_TYPE_PRESS:
			ev_u->type = 3;
			ev_u->_u.press.code = ev->press.code;
			break;

		case EVENT_TYPE_RELEASE:
			ev_u->type = 4;
			ev_u->_u.release.code = ev->release.code;
			break;

		case EVENT_TYPE_KEYREPEAT:
			ev_u->type = 5;
			ev_u->_u.keyrepeat.code = ev->keyrepeat.code;
			break;

		default:
			ev_u->type = 0;
	}
}


/*** UTILITY: CONVERT EVENT IDL TYPE TO DOPE EVENT ***
 *
 * \param ev_u  source dope event union
 * \param cmd   destination buffer for command string
 * \param ev    destination buffer for dope_event
 */
static void dope_event_u__to__dope_event(const dope_event_u *ev_u,
                                         char *cmd, dope_event *ev) {
	switch (ev_u->type) {

		/* command event */
		case 1:
			ev->type = EVENT_TYPE_COMMAND;
			ev->command.cmd = cmd;
			strncpy(cmd, ev_u->_u.command.cmd, EVENT_CMD_SIZE);
			break;

		/* motion event */
		case 2:
			ev->type = EVENT_TYPE_MOTION;
			ev->motion.rel_x = ev_u->_u.motion.rel_x;
			ev->motion.rel_y = ev_u->_u.motion.rel_y;
			ev->motion.abs_x = ev_u->_u.motion.abs_x;
			ev->motion.abs_y = ev_u->_u.motion.abs_y;
			break;

		/* press event */
		case 3:
			ev->type = EVENT_TYPE_PRESS;
			ev->press.code = ev_u->_u.press.code;
			break;

		/* release event */
		case 4:
			ev->type = EVENT_TYPE_RELEASE;
			ev->release.code = ev_u->_u.release.code;
			break;

		/* keyrepeat event */
		case 5:
			ev->type = EVENT_TYPE_KEYREPEAT;
			ev->keyrepeat.code = ev_u->_u.keyrepeat.code;
			break;

		/* undefined event */
		default:
			ev->type = EVENT_TYPE_UNDEFINED;
			break;
	}
}


int dopelib_init_eventqueue(int id) {
	dopelib_apps[id]->queue_sem = dopelib_sem_create(1);
	return dopelib_apps[id]->queue_sem ? 0 : -1;
}


long dopeapp_listener_event_component(CORBA_Object _dice_corba_obj,
                                      const dope_event_u *e,
                                      const char* bindarg,
                                      CORBA_Server_Environment *_dice_corba_env) {

	struct dopelib_app *app = dopelib_apps[(unsigned long)_dice_corba_env->user_data];
	long first = app->first;
	dope_event *event = &app->event_queue[first];

	/* do not handle events for non-initialized event queue */
	if (!app->queue_sem) {
		printf("%s: event queue for app_id=%d no initialized\n",
		       __FUNCTION__, (int)app->app_id);
		return -1;
	}

	app->first = (first + 1) % EVENT_QUEUE_SIZE;

	strncpy(app->bindarg_queue[first], bindarg, 250);

	dope_event_u__to__dope_event(e, &app->bindcmd_queue[first][0], event);

	/* signal incoming event */
	if (app->queue_sem) dopelib_sem_post(app->queue_sem);
	return 42;
}


/*** INTERFACE: RETURN NUMBER OF PENDING EVENTS ***/
int dope_events_pending(int id) {
	struct dopelib_app *app = get_app(id);
	return (app ? (app->first - app->last) : 0);
}


#if 0
/*** WAIT FOR AN EVENT ***/
void dopelib_wait_event(int id, dope_event **e_out,char **bindarg_out) {
	struct dopelib_app *app = get_app(id);
	long curr;

	if (!app) return;
	curr = app->last;

	/* wait for an incoming event */
	dopelib_sem_wait(app->queue_sem);
	app->last = (app->last + 1) % EVENT_QUEUE_SIZE;

	/* fill out the results of the function */
	if (bindarg_out) *bindarg_out = &app->bindarg_queue[curr][0];
	if (e_out) *e_out = &app->event_queue[curr];
}
#endif

void dope_inject_event(long app_id, dope_event *ev,
                       void (*callback)(dope_event *,void *), void *arg) {

	char buf[128];
	CORBA_Environment env = dice_default_environment;

	/* convert dope_event to idl-compatible event union */
	dope_event_u ev_u;
	dope_event__to__dope_event_u(ev, &ev_u);

	dopelib_callback_to_bindarg(callback, arg, buf, sizeof(buf));

	/* submit event to local listener server */
	dopeapp_listener_event_call(&dopelib_apps[app_id]->listener,
	                            &ev_u, buf, &env);

	if (DICE_IS_EXCEPTION(&env, CORBA_SYSTEM_EXCEPTION) &&
	    (DICE_EXCEPTION_MINOR(&env) == CORBA_DICE_EXCEPTION_IPC_ERROR))
	    printf ("IPC Error at client: 0x%02x\n", DICE_IPC_ERROR(&env));
}
