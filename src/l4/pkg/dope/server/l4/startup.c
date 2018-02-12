/*
 * \brief   DOpE L4 specific startup code
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

/*** GENERAL INCLUDES ***/
#include <stdio.h>
#include <stdlib.h>

/*** L4 INCLUDES ***/
#include <l4/util/getopt.h>

#if USE_RT_MON
#include <l4/rt_mon/histogram2d.h>

rt_mon_histogram2d_t * hist2dxy;
#endif

/*** L4 SPECIFIC CONFIG VARIABLES ***/

int config_use_vidfix   = 0;  /* certain graphic adapter deliver garbage in vesa info */
int config_events       = 0;  /* use Drops events to close DOpE applications          */
int config_oldresize    = 0;  /* use traditional way to resize windows                */
int config_adapt_redraw = 1;  /* adapt redraw to runtime measurements                 */
int config_menubar      = 0;  /* menubar is visible                                   */
int config_sticky       = 0;  /* windows are sticky                                   */
int config_nocursor     = 0;  /* no mouse cursor                                      */


/*** PLATFORM INDEPENDENT CONFIG VARIABLES ***/

extern int config_don_scheduler;  /* use donation scheduling    */
extern int config_transparency;   /* enable transparent windows */
extern int config_clackcommit;    /* commit on mouse release    */
extern int config_winborder;      /* size of window border      */


/*** GLOBAL L4 SPECIFIC VARIABLES ***/

void native_startup(int, char **);


/* Make sure that the jiffies symbol is taken from libio.a not libinput.a. */
asm (".globl jiffies");


/*** PARSE COMMAND LINE ARGUMENTS AND SET GLOBAL CONFIG VARIABLES ***/
static void do_args(int argc, char **argv) {
	signed char c;

	static struct option long_options[] = {
		{"vidfix",        0, 0, 'f'},
		{"donscheduler",  0, 0, 'd'},
		{"transparency",  0, 0, 't'},
		{"events",        0, 0, 'e'},
		{"clackcommit",   0, 0, 'c'},
		{"oldresize",     0, 0, 'r'},
		{"winborder",     1, 0, 'b'},
		{"menubar",       0, 0, 'm'},
		{"sticky",        0, 0, 's'},
		{"nocursor",      0, 0, 'n'},
		{0, 0, 0, 0}
	};

	/* read command line arguments */
	while (1) {
		c = getopt_long(argc, argv, "fdtecrb:msn", long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case 'f':
				config_use_vidfix = 1;
				printf("DOpE(init): using video fix\n");
				break;
			case 'd':
				config_don_scheduler = 1;
				printf("DOpE(init): using don scheduler\n");
				break;
			case 't':
				config_transparency = 1;
				printf("DOpE(init): using transparency\n");
				break;
			case 'e':
				config_events = 1;
				printf("DOpE(init): using events mechanism\n");
				break;
			case 'c':
				config_clackcommit = 1;
				printf("DOpE(init): commit on mouse release\n");
				break;
			case 'r':
				config_oldresize = 1;
				printf("DOpE(init): using old way of resizing windows\n");
				break;
			case 'm':
				config_menubar = 1;
				printf("DOpE(init): displaying menubar\n");
				break;
			case 's':
				config_sticky = 1;
				printf("DOpE(init): sticky windows\n");
				break;
			case 'n':
				config_nocursor = 1;
				printf("DOpE(init): no visible mouse cursor\n");
				break;
			case 'b':
				if (optarg) config_winborder = atol(optarg);
				printf("DOpE(init): using window border size of %d\n", config_winborder);
				break;
			default:
				printf("DOpE(init): unknown option!\n");
		}
	}
}


void native_startup(int argc, char **argv) {
#if USE_RT_MON
	{
		/* create a 2d histogram for dope repaint actions */
		double l[2] = {0, 0};         /* start at <0, 0> ...    */
		double h[2] = {400, 400};     /* ... up to <400, 400>   */
		int b[2] = {200, 200};        /* 200 bins for each dim */

		/*
		 * Create histogram with 2 layers and use TSC times.
		 *   layer 1 are accumulated times
		 *   layer 2 counts time number of events added together in layer 1
		 *   -> (layer 1 / layer 2) == average time
		 *
		 * Units: x- and y-axis are in pixel, z-axis is time in µsecs
		 */
		hist2dxy = rt_mon_hist2d_create(l, h, b, 2,
		                                "dope/rel_xy_2d",
		                                "w [px]", "h [px]", "t/px [ns/px]",
		                                RT_MON_TSC_TIME);
	}
#endif

	/*
	 * Just avoid to start DOpE if another instance is already
	 * present.
	 */
	do_args(argc, argv);
}
