/*
 * \brief   DOpE client library - event listener
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

#include "dopestd.h"
#include "dopeapp-server.h"
#include <l4/thread/thread.h>
#include "listener.h"
#include "app_struct.h"


void CORBA_Object_to_ident(CORBA_Object tid, char *dst, int dst_len) {
	snprintf(dst, dst_len, "t_id=0x%08X",
	         (int)tid->raw);
}


static void listener_thread(void *arg) {
	CORBA_Server_Environment dice_env = dice_default_server_environment;
	dice_env.user_data = arg;

	l4thread_started(NULL);

	dopeapp_listener_server_loop(&dice_env);
}


int dopelib_start_listener(long app_id) {

	struct dopelib_app *app = dopelib_apps[app_id];

	l4thread_t th = l4thread_create_named(listener_thread, ".listener",
	                                      (void *)app_id,
	                                      L4THREAD_CREATE_SYNC);
	app->listener = l4thread_l4_id(th);
	return 0;
}


void dopelib_stop_listener(long app_id) {

	struct dopelib_app *app = dopelib_apps[app_id];

	l4thread_shutdown(l4thread_id(app->listener));
}
