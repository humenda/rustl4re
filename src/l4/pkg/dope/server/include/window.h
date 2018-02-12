/*
 * \brief   Interface of DOpE Window widget module
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

#ifndef _DOPE_WINDOW_H_
#define _DOPE_WINDOW_H_

#include "widget.h"

struct window_methods;
struct window_data;

#define WIN_BORDERS 1
#define WIN_TITLE 2
#define WIN_CLOSER 4
#define WIN_FULLER 8

#define WINDOW struct window

struct window {
	struct widget_methods *gen;
	struct window_methods *win;
	struct widget_data    *wd;
	struct window_data    *wind;
};

struct window_methods {
	void    (*set_content)   (WINDOW *, WIDGET *content);
	WIDGET *(*get_content)   (WINDOW *);
	void    (*set_staytop)   (WINDOW *, s16 staytop_flag);
	s16     (*get_staytop)   (WINDOW *);
	void    (*set_elem_mask) (WINDOW *, s32 elem_mask);
	s32     (*get_elem_mask) (WINDOW *);
	void    (*set_title)     (WINDOW *, char *new_title);
	char   *(*get_title)     (WINDOW *);
	void    (*set_background)(WINDOW *, s16 bg_flag);
	s16     (*get_background)(WINDOW *);
	void    (*set_state)     (WINDOW *, s32 state);
	void    (*activate)      (WINDOW *);
	void    (*open)          (WINDOW *);
	void    (*close)         (WINDOW *);
	void    (*top)           (WINDOW *);
	void    (*handle_move)   (WINDOW *, WIDGET *);
	void    (*handle_resize) (WINDOW *, WIDGET *);
	void    (*set_x)         (WINDOW *, int x);
	void    (*set_y)         (WINDOW *, int y);
	void    (*set_kfocus)    (WINDOW *, WIDGETARG *kfocus);
	WIDGET *(*get_kfocus)    (WINDOW *);
};

struct window_services {
	WINDOW *(*create) (void);

	int shadow_left;    /* drop shadow size */
	int shadow_right;
	int shadow_top;
	int shadow_bottom;
};


#endif /* _DOPE_WINDOW_H_ */
