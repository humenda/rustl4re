/*
 * \brief   DOpE clipping handling module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This component handles clipping stacks. A
 * clipping area can be successively shrinked
 * to meet the needs of hierarchical widgets.
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
#include "clipping.h"

#define CLIPSTACK_SIZE 64

static long clip_x1, clip_y1, clip_x2, clip_y2;
static long cstack_x1[CLIPSTACK_SIZE];
static long cstack_y1[CLIPSTACK_SIZE];
static long cstack_x2[CLIPSTACK_SIZE];
static long cstack_y2[CLIPSTACK_SIZE];
static long csp;

int init_clipping(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** REQUEST FUNCTIONS ARE OFTEN CALLED - THE REASON FOR THE BUFFERED VALUES ***/
static long clip_get_x1 (void) {return clip_x1;}
static long clip_get_y1 (void) {return clip_y1;}
static long clip_get_x2 (void) {return clip_x2;}
static long clip_get_y2 (void) {return clip_y2;}


/*** SET (SHRINK) GLOBAL CLIPPING VALUES ***/
static  void clip_push(long x1, long y1, long x2, long y2) {
	if (csp >= CLIPSTACK_SIZE - 1) return;
	
	csp++;
	clip_x1 = cstack_x1[csp] = MAX(clip_x1, x1);
	clip_y1 = cstack_y1[csp] = MAX(clip_y1, y1);
	clip_x2 = cstack_x2[csp] = MIN(clip_x2, x2);
	clip_y2 = cstack_y2[csp] = MIN(clip_y2, y2);
}


/*** RESTORE PREVIOUS CLIPPING STATE ***/
static void clip_pop(void) {
	if (csp <= 0) return;

	csp--;
	clip_x1 = cstack_x1[csp];
	clip_y1 = cstack_y1[csp];
	clip_x2 = cstack_x2[csp];
	clip_y2 = cstack_y2[csp];
}


/*** SET GLOBAL CLIPPING VALUES TO WHOLE SCREEN ***/
static void clip_reset(void) {
	csp = 0;
	clip_x1 = cstack_x1[0];
	clip_y1 = cstack_y1[0];
	clip_x2 = cstack_x2[0];
	clip_y2 = cstack_y2[0];
}


/*** SET CLIPPING RANGE (SCREEN DIMENSIONS) ***/
static void clip_set_range(long x1, long y1, long x2, long y2) {
	csp = 0;
	clip_x1 = cstack_x1[0] = x1;
	clip_y1 = cstack_y1[0] = y1;
	clip_x2 = cstack_x2[0] = x2;
	clip_y2 = cstack_y2[0] = y2;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct clipping_services services = {
	clip_push,
	clip_pop,
	clip_reset,
	clip_set_range,
	clip_get_x1,
	clip_get_y1,
	clip_get_x2,
	clip_get_y2
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_clipping(struct dope_services *d) {

	d->register_module("Clipping 1.0",&services);
	return 1;
}
