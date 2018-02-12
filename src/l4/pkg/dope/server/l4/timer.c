/*
 * \brief   DOpE timer module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
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

#include <unistd.h>

#include <l4/re/env.h>

#include "timer.h"

int init_timer(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** RETURN CURRENT SYSTEM TIME COUNTER IN MICROSECONDS ***/
static u32 get_time(void) {
	return (u32)l4_kip_clock_lw(l4re_kip());
}


/*** RETURN DIFFERENCE BETWEEN TWO TIMES ***/
static u32 get_diff(u32 time1, u32 time2) {

	/* overflow check */
	if (time1>time2) {
		time1 -= time2;
		return (u32)0xffffffff - time1;
	}
	return time2-time1;
}


/*** WAIT THE SPECIFIED NUMBER OF MICROSECONDS ***/
static void __usleep(u32 num_usec) {
	usleep(num_usec);
}

/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct timer_services services = {
	get_time,
	get_diff,
	__usleep,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_timer(struct dope_services *d) {
#if 0
#ifndef ARCH_arm
	l4_calibrate_tsc(l4re_kip());
#endif
#endif

	d->register_module("Timer 1.0", &services);
	return 1;
}
