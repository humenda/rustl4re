/*
 * \brief   DOpE messenger module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module enables DOpE to tell its clients about
 * events. It uses DICE to communicate.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <stdio.h>
#include <stdlib.h>

#include "dopestd.h"
#include "event.h"
#include "appman.h"
#include "messenger.h"

#include <l4/dope/dopelib.h>
#include <l4/input/libinput.h>
#include <l4/re/c/event.h>
#include <l4/re/env.h>

static struct appman_services *appman;

int init_messenger(struct dope_services *d);

static void send_input_event(s32 app_id, EVENT *e, unsigned long bindarg)
{
  extern void dope_event_inject(s32, EVENT *, unsigned);
  e->stream_id = bindarg;
  dope_event_inject(app_id, e, 1);

#if 0
	struct l4input de;
	//CORBA_Environment env = dice_default_environment;
	//CORBA_Object listener = appman->get_listener(app_id);

	INFO(printf("Messenger(send_input_event)\n");)
	if (!listener || !e || !bindarg) return;

	switch (e->type) {

	case EVENT_MOUSE_ENTER:
		de.type = 1;
		de._u.command.cmd = "enter";
		break;

	case EVENT_MOUSE_LEAVE:
		de.type = 1;
		de._u.command.cmd = "leave";
		break;

	case EVENT_MOTION:
		de.type = 2;
		de._u.motion.rel_x = e->rel_x;
		de._u.motion.rel_y = e->rel_y;
		de._u.motion.abs_x = e->abs_x;
		de._u.motion.abs_y = e->abs_y;
		break;

	case EVENT_PRESS:
		de.type = 3;
		de._u.press.code = e->code;
		break;

	case EVENT_RELEASE:
		de.type = 4;
		de._u.release.code = e->code;
		break;

	case EVENT_KEY_REPEAT:
		de.type = 5;
		de._u.keyrepeat.code = e->code;
		break;

	default:
		return;
	}

#ifndef L4API_linux
	env.timeout = l4_ipc_timeout(781, 6, 781, 6);
#endif

	INFO(printf("Messenger(send_event): event type = %d\n",(int)de.type));
	INFO(printf("Messenger(send_event): try to deliver event\n");)
	dopeapp_listener_event_call(listener, &de, bindarg, &env);
	INFO(printf("Messenger(send_event): oki\n");)
#endif
}


static void send_action_event(s32 app_id, char *action, unsigned long bindarg)
{
  (void)app_id;
  (void)action;
  (void)bindarg;
  INFO(printf("%s %d action:%s arg:%lx\n", __func__, __LINE__, action, bindarg);)


#if 0
	dope_event_u de;
	CORBA_Environment env = dice_default_environment;
	CORBA_Object listener = appman->get_listener(app_id);

	if (!listener || !action  || !bindarg) return;
	de.type = 1;
	de._u.command.cmd = action;

#ifndef L4API_linux
	env.timeout = l4_ipc_timeout(781, 6, 781, 6);
#endif

	dopeapp_listener_event_call(listener,&de,bindarg,&env);
#endif
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct messenger_services services = {
	send_input_event,
	send_action_event,
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_messenger(struct dope_services *d) {

	appman = d->get_module("ApplicationManager 1.0");

	d->register_module("Messenger 1.0",&services);
	return 1;
}
