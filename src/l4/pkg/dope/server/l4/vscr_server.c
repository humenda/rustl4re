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


#include <stdio.h>

#include "dopestd.h"
#include "thread.h"
#include "vscreen.h"
#include "vscr_server.h"

#include <l4/util/util.h>

#define MAX_IDENTS 40

static struct thread_services *thread;

//static u8 ident_tab[MAX_IDENTS];    /* vscreen server identifier table */
static s16 thread_started=0;

int init_vscr_server(struct dope_services *d);


/*****************************
 *** VSCREEN WIDGET SERVER ***
 *****************************/

#if 0
void dope_vscr_waitsync_component(void *_dice_corba_obj,
                                  void *_dice_corba_env)
{
  printf("%s %d\n", __func__, __LINE__); 
  VSCREEN *vs = (VSCREEN *) _dice_corba_env->user_data;
  vs->vscr->waitsync(vs);
}

void dope_vscr_refresh_component(void *_dice_corba_obj,
                                 int x,
                                 int y,
                                 int w,
                                 int h,
                                 void *_dice_corba_env)
{
  printf("%s %d\n", __func__, __LINE__); 
  VSCREEN *vs = (VSCREEN *) _dice_corba_env->user_data;
  vs->vscr->refresh(vs, x, y, w, h);
}
#endif

static void *vscreen_server_thread(void *arg)
{
  (void)arg;
  printf("%s %d\n", __func__, __LINE__); 
#if 0
	int i;
	char ident_buf[10];
	VSCREEN *vs = (VSCREEN *)arg;
	CORBA_Server_Environment dice_env = dice_default_server_environment;

	dice_env.user_data = vs;
	INFO(printf("VScreen(server_thread): entered server thread\n"));

	l4thread_set_prio(l4thread_myself(), l4thread_get_prio(l4thread_myself())-5);

//  INFO(printf("VScreen(server_thread): tid = %lu.%lu\n",
//              (long)(vs->server_tid.id.task),
//              (long)(vs->server_tid.id.lthread)));

	INFO(printf("VScreen(server_thread): find server identifier\n"));

	/* find free identifier for this vscreen server */
	for (i=0;(i<MAX_IDENTS) && (ident_tab[i]);i++);
	if (i<MAX_IDENTS-1) {
		sprintf(ident_buf, "Dvs%d", i);
		ident_tab[i]=1;
	} else {
		/* if there are not enough identifiers, exit the server thread */
		thread_started=1;
		ERROR(printf("VScreen(server_thread): no free identifiers\n"));
		return;
	}

	INFO(printf("VScreen(server_thread): ident_buf=%s\n", ident_buf));
	if (!names_register(ident_buf)) {
		thread_started = 1;
		return;
	}

	vs->vscr->reg_server(vs, ident_buf);
	thread_started = 1;
	INFO(printf("VScreen(server_thread): thread successfully started.\n"));
	dice_env.timeout = l4_ipc_timeout(250, 2, 0, 0); /* send timeout 1ms */
	dope_vscr_server_loop(&dice_env);
#endif
	return NULL;
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** START VSCREEN SERVER THREAD ***
 *
 * \return 0 on success
 */
static int start(THREAD *tid, VSCREEN *vscr_widget)
{
  int wait_cnt = 0;
  int wait_max = 10; /* wait 10 iterations for the server thread to come up */
  int result;

  thread_started = 0;
  result = thread->start_thread(tid, &vscreen_server_thread, (void *)vscr_widget);

  if (result != 0) return result;

  /* wait some time for shaking hands */
  while (!thread_started) {
    l4_usleep(1000*10);
    wait_cnt++;
  }

  /* thread refused to shake hands? */
  if (wait_cnt == wait_max) return -1;

  return 0;
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
	d->register_module("VScreenServer 1.0", &services);
	return 1;
}
