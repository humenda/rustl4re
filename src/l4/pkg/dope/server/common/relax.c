/*
 * \brief   DOpE relaxation module
 * \date    2004-08-20
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
#include "relax.h"

int init_relax(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** DEFINE DURATION OF RELAXATION PROCESS ***/
static void set_duration(RELAX *r, float time) {
	float delta = (r->curr > r->dst) ? r->curr - r->dst : r->dst - r->curr;
	if (time == 0) time = 0.00001;
	r->accel = 4*delta/(time*time);

	/* assure an acceleration of at least 1.0 */
	if (r->accel < 1.0) r->accel = 1.0;
}


/*** DO A RELAXATION ITERATION ***/
static int relax_do_relax(RELAX *r) {
	float delta;

	/* destination value reached? */
	if (r->curr == r->dst) {
		r->speed = 0;
		return 0;
	}

	/* change value with current speed */
	if (r->curr > r->dst) {
		r->curr -= r->speed;
		if (r->curr < r->dst) r->curr = r->dst;
	} else {
		r->curr += r->speed;
		if (r->curr > r->dst) r->curr = r->dst;
	}

	/* adapt speed */
	delta = r->curr - r->dst;
	if (delta < 0) delta = -delta;

	if (r->speed * r->speed < delta * r->accel)
		r->speed += r->accel;
	else
		r->speed -= r->accel;

	if (r->speed < 1) r->speed = 1;

	return 1;  /* relaxation in progress */
};


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct relax_services services = {
	set_duration,
	relax_do_relax,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_relax(struct dope_services *d) {
	d->register_module("Relax 1.0", &services);
	return 1;
}
