/*
 * \brief   DOpE Linux specific startup code
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

void native_startup(int argc, char **argv);

int config_oldresize    = 0;  /* use traditional way to resize windows */
int config_exg_z_y      = 1;  /* exchange z and y keys                 */
int config_adapt_redraw = 0;  /* do not adapt redraw to duration time  */
int config_menubar      = 1;  /* menubar is visible                    */

void native_startup(int argc, char **argv) {
	/* there is no system dependent startup action */
	/* to do when using linux... */
}
