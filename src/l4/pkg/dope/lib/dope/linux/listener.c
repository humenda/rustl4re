/*
 * \brief   DOpE client library - Linux specific event listener
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

/*** LINUX INCLUDES ***/
#include <pthread.h>

/*** DOpE INCLUDES ***/
#include <dopeapp-server.h>
#include "dopelib.h"

/*** LOCAL INCLUDES ***/
#include "listener.h"
#include "sync.h"
#include "dopestd.h"
#include "app_struct.h"

extern int get_free_port(void);


void CORBA_Object_to_ident(CORBA_Object obj, char *dst, int dst_len) {
	snprintf(dst, dst_len, "%d", obj->sin_port);
}


static void *listener_thread(void *arg) {

	CORBA_Environment listener_env = dice_default_server_environment;

	struct dopelib_app *app = dopelib_apps[(int)arg];

	listener_env.srv_port  = app->listener.sin_port;
	listener_env.user_data = arg;

	dopeapp_listener_server_loop(&listener_env);
	return NULL;
}


int dopelib_start_listener(long app_id) {

	struct dopelib_app *app = dopelib_apps[app_id];
	pthread_t *listener_tid = (pthread_t *)malloc(sizeof(pthread_t));

	/* choose free port for the listener */
	app->listener_ptid = listener_tid;
	app->listener.sin_family = AF_INET;
	app->listener.sin_port = get_free_port();
	inet_aton("127.0.0.1", &app->listener.sin_addr);

	pthread_create(listener_tid, NULL, listener_thread, (void *)app_id);
	return 0;
}


void dopelib_stop_listener(long app_id) {

	struct dopelib_app *app = dopelib_apps[app_id];
	pthread_t *listener_tid = (pthread_t *)app->listener_ptid;
	pthread_cancel(*listener_tid);
}

