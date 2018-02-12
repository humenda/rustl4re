/*
 * \brief   DOpE VScreen server module
 * \date    2002-01-04
 * \author  Norman Feske <no@atari.org>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "vscr-server.h"
#include "dopestd.h"
#include "thread.h"
#include "timer.h"
#include "vscreen.h"
#include "vscr_server.h"

static struct timer_services  *timer;
static struct thread_services *thread;

static s16 thread_started = 0;

int init_vscr_server(struct dope_services *d);


/*****************************
 *** VSCREEN WIDGET SERVER ***
 *****************************/

void dope_vscr_waitsync_component(CORBA_Object _dice_corba_obj,
                                        CORBA_Environment *_dice_corba_env) {
	VSCREEN *vs = (VSCREEN *) _dice_corba_env->user_data;
	vs->vscr->waitsync(vs);
}

void dope_vscr_refresh_component(CORBA_Object _dice_corba_obj,
                                 int x,
                                 int y,
                                 int w,
                                 int h,
                                 CORBA_Environment *_dice_corba_env) {

	VSCREEN *vs = (VSCREEN *) _dice_corba_env->user_data;
	vs->vscr->refresh(vs,x,y,w,h);
}


static void vscreen_server_thread(void *arg) {
	char ident_buf[10];
	VSCREEN *vs = (VSCREEN *)arg;
	CORBA_Environment dice_env;
	int sh;
	struct sockaddr_in myaddr;
	size_t namelen;

	INFO(printf("VScreenServer(thread): entered server thread\n"));
	INFO(printf("VScreenServer(thread): find free port\n"));

	myaddr.sin_family = AF_INET;
	myaddr.sin_port = 0;
	myaddr.sin_addr.s_addr = INADDR_ANY;

	sh = socket (AF_INET, SOCK_STREAM, 0);
	bind(sh,(struct sockaddr *)&myaddr,sizeof(struct sockaddr_in));
	getsockname(sh,(struct sockaddr *)&myaddr,&namelen);

	dice_env.srv_port = myaddr.sin_port;
	dice_env.user_data = vs;
	close(sh);

	printf("VScreenServer(thread): got port number %d\n",(int)myaddr.sin_port);

	sprintf(ident_buf,"%d", dice_env.srv_port);
	vs->vscr->reg_server(vs,ident_buf);
	thread_started = 1;
	INFO(printf("VScreenServer(thread): thread successfully started.\n"));
	dope_vscr_server_loop(&dice_env);
}



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static int start(THREAD *t, VSCREEN *vscr_widget) {
	int result;
	thread_started = 0;

	/* start server thread */
	result = thread->start_thread(t, &vscreen_server_thread, (void *)vscr_widget);

	/* shake hands with newly created thread */
	if (result == 0)
		while (!thread_started && (timer)) timer->usleep(1000);

	return result;
}



/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct vscr_server_services services = {
	start
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/


int init_vscr_server(struct dope_services *d) {

	thread = d->get_module("Thread 1.0");
	timer  = d->get_module("Timer 1.0");

	d->register_module("VScreenServer 1.0",&services);
	return 1;
}
