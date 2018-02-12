/*
 * \brief   DOpE server module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module is the main communication interface
 * between DOpE clients and DOpE.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <stdlib.h>   /* only for strtol */
#include <dope-server.h>

/*** DOPE SPECIFIC ***/
#include "dopestd.h"
#include "thread.h"
#include "server.h"
#include "appman.h"
#include "script.h"
#include "scope.h"
#include "screen.h"
#include "scheduler.h"
#include "userstate.h"

int init_server(struct dope_services *d);

static struct scope_services     *scope;
static struct thread_services    *thread;
static struct appman_services    *appman;
static struct script_services    *script;
static struct screen_services    *screen;
static struct scheduler_services *scheduler;
static struct userstate_services *userstate;

long dope_manager_init_app_component(CORBA_Object _dice_corba_obj,
                                           const char* appname,
                                           const char* listener,
                                           CORBA_Environment *_dice_corba_env) {
	CORBA_Object l = malloc(sizeof(struct sockaddr));
	s32 app_id = appman->reg_app(appname);
	SCOPE *rootscope = scope->create();
	appman->set_rootscope(app_id, rootscope);
	if (!l) {
		ERROR(printf("Server(dope_manager_init_app): out of memory!\n"););
		return 0;
	}
	l->sin_family = AF_INET;
	l->sin_port = strtol(listener, NULL, 10);
	inet_aton("127.0.0.1", &l->sin_addr);
	appman->reg_listener(app_id, l);
	INFO(printf("Server(init_app): application init requested. appname=%s listener=%s\n", appname, listener);)
	
	return app_id;
}


void dope_manager_deinit_app_component(CORBA_Object _dice_corba_obj,
                                       long app_id,
                                       CORBA_Environment *_dice_corba_env) {
	INFO(printf("Server(deinit_app): application (id=%lu) deinit requested\n", app_id);)
	scheduler->release_app(app_id);
	userstate->release_app(app_id);
	screen->forget_children(app_id);
	appman->unreg_app(app_id);
}


long dope_manager_exec_cmd_component(CORBA_Object _dice_corba_obj,
                                           long app_id,
                                           const char* cmd,
                                           CORBA_Environment *_dice_corba_env) {
	int ret;
	INFO(printf("Server(exec_cmd): cmd %s execution requested by app_id=%lu\n", cmd, app_id);)
	appman->lock(app_id);
	ret = script->exec_command(app_id, (char *)cmd, NULL, 0);
	appman->unlock(app_id);
	if (ret < 0) printf("DOpE(exec_cmd): Error - command \"%s\" returned %d\n", cmd, ret);
	return ret;
}


long dope_manager_exec_req_component(CORBA_Object _dice_corba_obj,
                                     long app_id,
                                     const char* cmd,
                                     char result[256],
                                     int *res_len,
                                     CORBA_Environment *_dice_corba_env) {
	int ret;

	INFO(printf("Server(exec_req): cmd %s execution requested by app_id=%lu\n", cmd, (u32)app_id);)
	appman->lock(app_id);
	result[0] = 0;
	ret = script->exec_command(app_id, (char *)cmd, &result[0], *res_len);
	appman->unlock(app_id);
	INFO(printf("Server(exec_req): send result msg: %s\n", result));

	if (ret < 0) printf("DOpE(exec_req): Error - command \"%s\" returned %d\n", cmd, ret);
	result[255] = 0;
	return ret;
}

long dope_manager_get_keystate_component(CORBA_Object _dice_corba_obj,
                                               long keycode,
                                               CORBA_Environment *_dice_corba_env) {
	return userstate->get_keystate(keycode);
}


char dope_manager_get_ascii_component(CORBA_Object _dice_corba_obj,
                                            long keycode,
                                            CORBA_Environment *_dice_corba_env) {
	return userstate->get_ascii(keycode);
}


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

static void server_thread(void *arg) {
	CORBA_Environment dopesrv_env = dice_default_server_environment;
	INFO(printf("Server(server_thead): entering server mainloop\n");)
	dopesrv_env.srv_port = htons(9997);
	dope_manager_server_loop(&dopesrv_env);
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** START SERVING ***/
static void start(void) {
	INFO(printf("Server(start): creating server thread\n");)
	thread->start_thread(NULL, &server_thread, NULL);
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct server_services services = {
	start,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_server(struct dope_services *d) {

	thread    = d->get_module("Thread 1.0");
	appman    = d->get_module("ApplicationManager 1.0");
	script    = d->get_module("Script 1.0");
	userstate = d->get_module("UserState 1.0");
	scope     = d->get_module("Scope 1.0");
	screen    = d->get_module("Screen 1.0");
	scheduler = d->get_module("Scheduler 1.0");

	d->register_module("Server 1.0", &services);
	return 1;
}
