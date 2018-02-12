/*
 * \brief   DOpE client library - DICE specific functions
 * \date    2003-02-28
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

/*** GENERAL INCLUDES ***/
#include <stdio.h>
#include <stdarg.h>

/*** LOCAL INCLUDES ***/
#include "dopelib.h"
#include "dope-client.h"
#include "dopestd.h"
#include "app_struct.h"
#include "sync.h"
#include "listener.h"
#include "events.h"
#include "init.h"

extern CORBA_Object dope_server;

struct dopelib_app *dopelib_apps[MAX_DOPE_CLIENTS];

static struct dopelib_app first_app;


/*** INTERFACE: REGISTER NEW DOpE APPLICATION ***/
long dope_init_app(const char *app_name) {
	int id;
	DICE_DECLARE_ENV(_env);
	struct dopelib_app *app;
	char buf[128];

	for (id=0; (id<MAX_DOPE_CLIENTS) && (dopelib_apps[id]); id++);
	if (id >= MAX_DOPE_CLIENTS) {
		ERROR(printf("DOpElib(init_app): error: no free id found\n"));
		return -1;
	}

	/* use static struct for first application to avoid memory allocation in most cases */
	if (id == 0) {
		app = &first_app;
	} else {
		app = (struct dopelib_app *)malloc(sizeof(struct dopelib_app));
		if (!app) {
			ERROR(printf("DOpElib(init_app): error: out of memory\n"));
			return -1;
		}
		memset(app, 0, sizeof(struct dopelib_app));
	}
	dopelib_apps[id] = app;

	/* init event queue that is shared between listener and main thread */
	if (dopelib_init_eventqueue(id) < 0) return -1;

	dopelib_start_listener(id);

	CORBA_Object_to_ident(&app->listener, buf, sizeof(buf));
	app->app_id = dope_manager_init_app_call(dope_server, app_name, buf, &_env);
	return id;
}


/*** INTERFACE: UNREGISTER DOpE APPLICATION ***/
long dope_deinit_app(long id) {
	struct dopelib_app *app = dopelib_apps[id];
	DICE_DECLARE_ENV(_env);
	if (!app) return -1;

	/* notify DOpE to destroy the application's namespace */
	dope_manager_deinit_app_call(dope_server, app->app_id, &_env);

	dopelib_stop_listener(id);

	/* free local identifier */
	dopelib_apps[id] = NULL;
	if (app != &first_app) free(app);
	
	return 0;
}



/*** INTERFACE: EXEC DOPE COMMAND AND REQUEST RESULT ***
 *
 * \param id       virtual DOpE application id
 * \param res      destination buffer for storing the result
 * \param res_max  maximum length of result
 * \param command  DOpE command to execute
 */
int dope_req(long id, char *res, int res_max, const char *cmd) {
	struct dopelib_app *app = dopelib_apps[id];
	DICE_DECLARE_ENV(_env);
	if (!app || !cmd || !dope_server) return -1;
	return dope_manager_exec_req_call(dope_server, app->app_id,
	                                  cmd, res, &res_max, &_env);
}


/*** INTERFACE: EXEC DOpE FORMAT STRING COMMAND AND REQUEST RESULT ***/
int dope_reqf(long app_id, char *res, int res_max, const char *format, ...) {
	int ret;
	va_list list;
	static char cmdstr[1024];

	dopelib_mutex_lock(dopelib_cmdf_mutex);
	va_start(list, format);
	vsnprintf(cmdstr, 1024, format, list);
	va_end(list);
	ret = dope_req(app_id, res, res_max, cmdstr);
	dopelib_mutex_unlock(dopelib_cmdf_mutex);
	
	return ret;
}


/*** INTERFACE: SHORTCUT EXEC A DOPE COMMAND WITHOUT RECEIVING THE RESULT ***
 *
 * \param id       virtual DOpE application id
 * \param command  DOpE command to execute
 * \return         0 on success
 *
 * This function actually receives the result from the DOpE server
 * but does not provide it to the caller.
 */
int dope_cmd(long id, const char *cmd) {
	DICE_DECLARE_ENV(_env);
	if (!cmd || (id < 0) || (id >= MAX_DOPE_CLIENTS) || !dopelib_apps[id]) return -1;
	return dope_manager_exec_cmd_call(dope_server, dopelib_apps[id]->app_id, cmd, &_env);
}


/*** INTERFACE: EXEC DOpE FORMAT STRING COMMAND ***/
int dope_cmdf(long app_id, const char *format, ...) {
	int ret;
	va_list list;
	static char cmdstr[1024];

	dopelib_mutex_lock(dopelib_cmdf_mutex);
	va_start(list, format);
	vsnprintf(cmdstr, 1024, format, list);
	va_end(list);
	ret = dope_cmd(app_id, cmdstr);
	dopelib_mutex_unlock(dopelib_cmdf_mutex);

	return ret;
}


long dope_get_keystate(long id, long keycode) {
	DICE_DECLARE_ENV(_env);
	return dope_manager_get_keystate_call(dope_server, keycode, &_env);
}


char dope_get_ascii(long id, long keycode) {
	DICE_DECLARE_ENV(_env);
	return dope_manager_get_ascii_call(dope_server, keycode, &_env);
}
