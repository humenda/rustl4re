/*
 * \brief   DOpE entry function
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This file describes the startup of DOpE. It
 * initialises all needed modules and calls the
 * eventloop (see eventloop.c) of DOpE.
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
#include "scheduler.h"
#include "server.h"
#include "gfx.h"
#include "userstate.h"
#include "screen.h"

static struct scheduler_services *sched;
static struct gfx_services       *gfx;
static struct server_services    *server;
static struct screen_services    *screen;
static struct userstate_services *userstate;

extern SCREEN *curr_scr;

/*** PROTOTYPES FROM STARTUP.C (IN SYSTEM DEPENDENT DIRECTORY) ***/
extern void native_startup(int argc, char **argv);

/*** PROTOTYPES FROM POOL.C ***/
extern long  pool_add(const char *name,void *structure);
extern void *pool_get(const char *name);

/*** PROTOTYPES FROM 'MODULES' ***/
extern int init_keymap           (struct dope_services *);
extern int init_clipping         (struct dope_services *);
extern int init_scrdrv           (struct dope_services *);
extern int init_input            (struct dope_services *);
extern int init_viewman          (struct dope_services *);
extern int init_widman           (struct dope_services *);
extern int init_screen           (struct dope_services *);
extern int init_timer            (struct dope_services *);
extern int init_tick             (struct dope_services *);
extern int init_relax            (struct dope_services *);
extern int init_button           (struct dope_services *);
extern int init_entry            (struct dope_services *);
extern int init_loaddisplay      (struct dope_services *);
extern int init_variable         (struct dope_services *);
extern int init_label            (struct dope_services *);
extern int init_background       (struct dope_services *);
extern int init_container        (struct dope_services *);
extern int init_window           (struct dope_services *);
extern int init_userstate        (struct dope_services *);
extern int init_conv_fnt         (struct dope_services *);
extern int init_fontman          (struct dope_services *);
extern int init_gfx              (struct dope_services *);
extern int init_gfxscr16         (struct dope_services *);
extern int init_gfximg16         (struct dope_services *);
extern int init_gfximg32         (struct dope_services *);
extern int init_gfximgyuv420     (struct dope_services *);
extern int init_cache            (struct dope_services *);
extern int init_scale            (struct dope_services *);
extern int init_scrollbar        (struct dope_services *);
extern int init_frame            (struct dope_services *);
extern int init_grid             (struct dope_services *);
extern int init_redraw           (struct dope_services *);
extern int init_simple_scheduler (struct dope_services *);
//extern int init_don_scheduler    (struct dope_services *);
extern int init_hashtable        (struct dope_services *);
extern int init_thread           (struct dope_services *);
extern int init_tokenizer        (struct dope_services *);
extern int init_scope            (struct dope_services *);
extern int init_script           (struct dope_services *);
extern int init_appman           (struct dope_services *);
extern int init_server           (struct dope_services *);
extern int init_winlayout        (struct dope_services *);
extern int init_messenger        (struct dope_services *);
extern int init_vscreen          (struct dope_services *);
extern int init_vscr_server      (struct dope_services *);
extern int init_vtextscreen      (struct dope_services *);
extern int init_sharedmem        (struct dope_services *);

/*** PROTOTYPES FROM EVENTLOOP.C ***/
extern void eventloop(struct dope_services *);

struct dope_services dope = {
	pool_get,
	pool_add,
};

/*** WE HAVE TO IMPLEMENT THIS SOMEWHERE ***/
void *CORBA_alloc(long);
void *CORBA_alloc(long size) {
	return malloc(size);
}

int config_transparency  = 0;   /* use translucent effects                */
int config_don_scheduler = 0;   /* use donation scheduler                 */
int config_clackcommit   = 0;   /* deliver commit events on mouse release */
int config_winborder     = 5;   /* size of window resize border           */

int main(int argc,char **argv) {
	INFO(char *dbg="Main(init): ");

	native_startup(argc, argv);

	/*** init modules ***/

	INFO(printf("%sSharedMemory\n",dbg));
	init_sharedmem(&dope);

	INFO(printf("%sTimer\n",dbg));
	init_timer(&dope);

	INFO(printf("%sTick\n",dbg));
	init_tick(&dope);

	INFO(printf("%sRelax\n",dbg));
	init_relax(&dope);

	INFO(printf("%sKeymap\n",dbg));
	init_keymap(&dope);

	INFO(printf("%sThread\n",dbg));
	init_thread(&dope);

	INFO(printf("%sCache\n",dbg));
	init_cache(&dope);

	INFO(printf("%sHashTable\n",dbg));
	init_hashtable(&dope);

	INFO(printf("%sApplication Manager\n",dbg));
	init_appman(&dope);

	INFO(printf("%sTokenizer\n",dbg));
	init_tokenizer(&dope);

	INFO(printf("%sMessenger\n",dbg));
	init_messenger(&dope);

	INFO(printf("%sScript\n",dbg));
	init_script(&dope);

	INFO(printf("%sClipping\n",dbg));
	init_clipping(&dope);

	INFO(printf("%sScreen Driver\n",dbg));
	init_scrdrv(&dope);

	INFO(printf("%sInput\n",dbg));
	init_input(&dope);

	INFO(printf("%sViewManager\n",dbg));
	init_viewman(&dope);

	INFO(printf("%sConvertFNT\n",dbg));
	init_conv_fnt(&dope);

	INFO(printf("%sFontManager\n",dbg));
	init_fontman(&dope);

	INFO(printf("%sGfxScreen16\n",dbg));
	init_gfxscr16(&dope);

	INFO(printf("%sGfxImage16\n",dbg));
	init_gfximg16(&dope);

	INFO(printf("%sGfxImage32\n",dbg));
	init_gfximg32(&dope);

	INFO(printf("%sGfxImageYUV420\n",dbg));
	init_gfximgyuv420(&dope);

	INFO(printf("%sGfx\n",dbg));
	init_gfx(&dope);

	INFO(printf("%sRedrawManager\n",dbg));
	init_redraw(&dope);

	INFO(printf("%sUserState\n",dbg));
	init_userstate(&dope);

	INFO(printf("%sWidgetManager\n",dbg));
	init_widman(&dope);

	INFO(printf("%sScope\n",dbg));
	init_scope(&dope);

	INFO(printf("%sButton\n",dbg));
	init_button(&dope);

	INFO(printf("%sEntry\n",dbg));
	init_entry(&dope);

	INFO(printf("%sVariable\n",dbg));
	init_variable(&dope);

	INFO(printf("%sLabel\n",dbg));
	init_label(&dope);

	INFO(printf("%sLoadDisplay\n",dbg));
	init_loaddisplay(&dope);

	INFO(printf("%sBackground\n",dbg));
	init_background(&dope);

	INFO(printf("%sScrollbar\n",dbg));
	init_scrollbar(&dope);

	INFO(printf("%sScale\n",dbg));
	init_scale(&dope);

	INFO(printf("%sFrame\n",dbg));
	init_frame(&dope);

	INFO(printf("%sContainer\n",dbg));
	init_container(&dope);

	INFO(printf("%sGrid\n",dbg));
	init_grid(&dope);

	INFO(printf("%sWinLayout\n",dbg));
	init_winlayout(&dope);

	INFO(printf("%sWindow\n",dbg));
	init_window(&dope);

	INFO(printf("%sScreen\n",dbg));
	init_screen(&dope);

	INFO(printf("%sScheduler\n",dbg));
	if (config_don_scheduler)
        {
	//	init_don_scheduler(&dope);
          printf("NOOOOOOOOOOOOOOOOOOO\n");
        }
	else
		init_simple_scheduler(&dope);

	INFO(printf("%sVScreenServer\n",dbg));
	init_vscr_server(&dope);

	INFO(printf("%sVScreen\n",dbg));
	init_vscreen(&dope);
	
	INFO(printf("%sVTextScreen\n",dbg));
	init_vtextscreen(&dope);

	INFO(printf("%sServer\n",dbg));
	init_server(&dope);

	INFO(printf("%screate screen\n",dbg));
	{
		static GFX_CONTAINER *scr_ds;
		gfx       = pool_get("Gfx 1.0");
		screen    = pool_get("Screen 1.0");
		userstate = pool_get("UserState 1.0");

		scr_ds = gfx->alloc_scr("default");
		curr_scr = screen->create();
		curr_scr->scr->set_gfx(curr_scr, scr_ds);
		userstate->set_max_mx(gfx->get_width(scr_ds));
		userstate->set_max_my(gfx->get_height(scr_ds));
	}
	
	INFO(printf("%sstarting server\n",dbg));
	if ((server = pool_get("Server 1.0")))
		server->start();
	
	INFO(printf("%sstarting scheduler\n",dbg));
	if ((sched  = pool_get("Scheduler 1.0")))
		sched->process_mainloop();

	return 0;
}
