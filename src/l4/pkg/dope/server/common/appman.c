/*
 * \brief   DOpE application manager module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module is used to manage DOpE-clients.
 * It handles all needed application specific
 * information such as its name, variables etc.
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
#include "hashtab.h"
#include "thread.h"
#include "appman.h"
#include "screen.h"
#include "scope.h"

#define APP_NAMELEN       64    /* application identifier string length    */

static struct hashtab_services *hashtab;
static struct thread_services  *thread;
//static struct scope_services   *scope;

struct app {
	char name[APP_NAMELEN];     /* identifier string of the application    */
	THREAD  *app_thread;        /* application thread                      */
	SCOPE   *rootscope;         /* root scope of the application           */
	void    *listener;          /* associated result/event listener        */
	THREAD  *list_thread;       /* listener thread (optional)              */
};


extern SCREEN *curr_scr;

static struct app *apps[MAX_APPS];

int init_appman(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** RETURN THE FIRST FREE APPLICATION ID ***/
static s32 get_free_app_id(void) {
	u32 i;

	for (i=1; i<MAX_APPS; i++) {
		if (!apps[i]) return i;
	}
	INFO(printf("AppMan(get_free_app_id): no free dope application id!\n");)
	return -1;
}


/*** CREATE NEW APPLICATION ***/
static struct app *new_app(void) {
	struct app *new;

	/* get memory for application structure */
	new = zalloc(sizeof(struct app));
	if (!new) {
		INFO(printf("AppMan(new_app): out of memory!\n");)
		return NULL;
	}

	/* create hash table to store the variables of the application */
//	new->rootscope = (SCOPE *)scope->create();

	new->app_thread = thread->alloc_thread();
	return new;
}


/*** SET THE IDENTIFIER STRING OF AN APPLICATION ***/
static void set_app_name(struct app *app, const char *appname) {
	u32 i;

	if (!app || !appname) return;

	/* copy application name into application data structure */
	for (i=0;i<APP_NAMELEN;i++) {
		app->name[i]=appname[i];
		if (!appname[i]) break;
	}
	app->name[APP_NAMELEN-1]=0;
}


/*** TEST IF AN APP_ID IS A VALID ONE ***/
static u16 app_id_valid(u32 app_id) {

	if (app_id>=MAX_APPS) {
		INFO(printf("AppMan(app_id_value): invalid app_id (out of range)\n");)
		return 0;
	}
	if (!apps[app_id]) {
		INFO(printf("AppMan(unregister): invalid app_id (no application with this id)\n");)
		return 0;
	}
	return 1;
}


/*** RETURN NAME OF APPLICATION WITH THE SPECIFIED ID ***/
static char *get_app_name(s32 app_id) {
	struct app *app;
	if (!app_id_valid(app_id)) return "";
	app = apps[app_id];
	return app->name;
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** REGISTER NEW APPLICATION ***
 *
 * app_name: identifier string for the application
 * returns dope_client id
 */
static s32 register_app(const char *app_name) {
	s32 newapp_id;

	newapp_id = get_free_app_id();
	if (newapp_id < 0) {
		INFO(printf("AppMan(register): application registering failed (no free app id)\n");)
		return -1;
	}

	/* create data structures for the DOpE internal representation of the app */
	apps[newapp_id] = new_app();
	if (!apps[newapp_id]) {
		INFO(printf("AppMan(register): application registering failed.\n");)
		return -1;
	}

	set_app_name(apps[newapp_id], app_name);
	return newapp_id;
}


/*** UNREGISTER AN APPLICATION AND FREE ALL ASSOCIATED INTERNAL RESSOURCES ***/
static s32 unregister_app(u32 app_id) {
	struct app *app;
	if (!app_id_valid(app_id)) return -1;

	app = apps[app_id];
	if (!app) return -1;

	curr_scr->gen->lock((WIDGET *)curr_scr);

	/* prevent any events to be delivered anymore */
	app->listener = NULL;

	/* delete root namespace */
	if (app->rootscope) app->rootscope->gen->dec_ref((WIDGET *)app->rootscope);

	/* deallocate thread ids */
	if (app->list_thread) thread->free_thread(app->list_thread);
	if (app->app_thread)  thread->free_thread(app->app_thread);

	/* free the memory for the internal application representation */
	free(app);

	/* mark the corresponding app_id to be free */
	apps[app_id] = NULL;

	curr_scr->gen->unlock((WIDGET *)curr_scr);
	return 0;
}


/*** DEFINE ROOT SCOPE OF AN APPLICATION ***/
static void set_rootscope(u32 app_id, SCOPE *rootscope) {
	if (!app_id_valid(app_id) || !rootscope) return;
	apps[app_id]->rootscope = rootscope;
	if (curr_scr) curr_scr->gen->inc_ref((WIDGET *)curr_scr);
	rootscope->scope->set_var(rootscope, "Screen", "screen", 255, (WIDGET *)curr_scr);
}


/*** REQUEST ROOT SCOPE OF AN APPLICATION ***/
static SCOPE *get_rootscope(u32 app_id) {
	if (!app_id_valid(app_id)) return NULL;
	return apps[app_id]->rootscope;
}


/*** REGISTER EVENT LISTENER DESTINATION POINT OF AN APPLICATION ***
 *
 * The listener is not necessarily a thread. It could be a socket descriptor
 * or something else. We never know what hides behind the listener pointer.
 */
static void reg_listener(s32 app_id, void *listener) {
	if (!app_id_valid(app_id)) return;
	apps[app_id]->listener = listener;
}


/*** REGISTER EVENT LISTENER THREAD OF AN APPLICATION ***
 *
 * We only store this optional listener thread id for proper resource deallocation.
 */
static void reg_list_thread(s32 app_id, THREAD *listener_thread) {
	if (!app_id_valid(app_id)) return;
	apps[app_id]->list_thread = listener_thread;
}


/*** REQUEST EVENT LISTENER THREAD OF AN APPLICATION ***/
static void *get_listener(s32 app_id) {
	if (!app_id_valid(app_id)) return NULL;
	return apps[app_id]->listener;
}


/*** REGISTER APPLICATION THREAD ***/
static void reg_app_thread(s32 app_id, THREAD *app_thread) {
	if (!app_id_valid(app_id)) return;
	thread->copy_thread(app_thread, apps[app_id]->app_thread);
}


/*** REQUEST APPLICATION THREAD ***/
static THREAD *get_app_thread(s32 app_id) {
	if (!app_id_valid(app_id)) return NULL;
	return apps[app_id]->app_thread;
}


/*** FIND APPLICATION IF OF SPECIFIED APPLICATION THREAD ***/
static s32 app_id_of_thread(THREAD *app_thread) {
	u32 i;
	for (i=1; i<MAX_APPS; i++) {
		if (apps[i] && thread->thread_equal(apps[i]->listener, app_thread)) {
			INFO(printf("Appman(app_id_of_thread): found app_id, return %d\n", (int)i));
			return i;
		}
	}
	return -1;
}


/*** RESOLVE APPLICATION ID BY ITS NAME ***/
static s32 app_id_of_name(char *app_name) {
	u32 i;
	for (i=1; i<MAX_APPS; i++) {
		if (apps[i] && dope_streq(app_name, apps[i]->name, 255)) {
			return i;
		}
	}

	return -1;
}


/*** LOCK APPLICATION FOR MUTUAL EXCLUSIVE MODIFICATIONS ***/
static void lock(s32 app_id) {
    (void)app_id;
//	SCOPE *s;
//	if (!app_id_valid(app_id) || !apps[app_id]->rootscope) return;
//	s = apps[app_id]->rootscope;
//	s->gen->lock((WIDGET *)s);
////	thread->mutex_down(global_lock);
	if (curr_scr) {
		curr_scr->gen->lock((WIDGET *)curr_scr);
	} else {
		printf("AppMan(lock): lock not possible because curr_scr is not defined.\n");
	}
}


/*** UNLOCK APPLICATION ***/
static void unlock(s32 app_id) {
    (void)app_id;
//	SCOPE *s;
//	if (!app_id_valid(app_id) || !apps[app_id]->rootscope) return;
//	s = apps[app_id]->rootscope;
//	s->gen->unlock((WIDGET *)s);
////	thread->mutex_up(global_lock);
	if (curr_scr) curr_scr->gen->unlock((WIDGET *)curr_scr);
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct appman_services services = {
	register_app,
	unregister_app,
	set_rootscope,
	get_rootscope,
	reg_listener,
	reg_list_thread,
	get_listener,
	get_app_name,
	reg_app_thread,
	get_app_thread,
	app_id_of_thread,
	app_id_of_name,
	lock,
	unlock,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_appman(struct dope_services *d) {

	hashtab = d->get_module("HashTable 1.0");
	thread  = d->get_module("Thread 1.0");
//	scope   = d->get_module("Scope 1.0");

//	global_lock = thread->create_mutex(0);

	d->register_module("ApplicationManager 1.0", &services);
	return 1;
}
