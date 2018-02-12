/*
 * \brief   Interface of DOpE Screen widget module
 * \date    2003-11-16
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

#ifndef _DOPE_SCREEN_H_
#define _DOPE_SCREEN_H_

#include "widget.h"
#include "gfx.h"

struct screen_methods;
struct screen_data;

#define SCREEN struct screen

struct screen {
	struct widget_methods *gen;
	struct screen_methods *scr;
	struct widget_data    *wd;
	struct screen_data    *sd;
};

struct screen_methods {
	void (*set_gfx)     (SCREEN *scr, GFX_CONTAINER *ds);
	void (*place)       (SCREEN *scr, WIDGET *win, long x, long y, long w, long h);
	void (*remove)      (SCREEN *scr, WIDGET *win);
	void (*top)         (SCREEN *scr, WIDGET *win);
	void (*back)        (SCREEN *scr, WIDGET *win);
	void (*set_title)   (SCREEN *scr, WIDGET *win, char *title);
	void (*reorder)     (SCREEN *scr);
	void (*set_act_win) (SCREEN *scr, WIDGET *win);
};

struct screen_services {
	SCREEN *(*create) (void);

	/*
	 * When an application exists, we have to make sure
	 * that all its associated widgets get vanished from
	 * screen. The function forget_children wipes out
	 * all child widgets from all screens.
	 */
	void (*forget_children) (int app_id);
};

#define NOARG -2147483646   /* magic value to indicate the use of a default value */

#endif /* _DOPE_SCREEN_H_ */

