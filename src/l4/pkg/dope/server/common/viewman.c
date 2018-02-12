/*
 * \brief   DOpE view manager
 * \date    2004-09-03
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This is just a dummy implementation that is used for DOpE
 * as standalone window server.
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
#include "viewman.h"

int init_viewman(struct dope_services *d);


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** CREATE A NEW VIEW ***
 *
 * \return  view id of the new view or a negative error code
 */
static struct view *view_create(void) {
	return NULL;
}


/*** DESTROY VIEW ***/
static void view_destroy(struct view *v) { (void)v; }


/*** POSITION VIEW ***
 *
 * \return  0 on success or a negative error code
 */
static int view_place(struct view *v, int x, int y, int w, int h) {
  (void)v; (void)x; (void)y; (void)w; (void)h;
	return 0;
}


/*** BRING VIEW ON TOP ***
 *
 * \return  0 on success or a negative error code
 */
static int view_top(struct view *v) {
  (void)v;
	return 0;
}


/*** BRING VIEW ON BACK ***
 *
 * \return  0 on success or a negative error code
 */
static int view_back(struct view *v) {
  (void)v;
	return 0;
}


/*** SET TITLE OF A VIEW ***
 *
 * \return  0 on success or a negative error code
 */
static int view_set_title(struct view *v, const char *title) {
  (void)v; (void)title;
	return 0;
}


/*** DEFINE BACKGROUND VIEW ***
 *
 * \return  0 on success or a negative error code
 */
static int view_set_bg(struct view *v) {
  (void)v;
	return 0;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct viewman_services services = {
	view_create,
	view_destroy,
	view_place,
	view_top,
	view_back,
	view_set_title,
	view_set_bg,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_viewman(struct dope_services *d) {
	d->register_module("ViewManager 1.0", &services);
	return 1;
}
