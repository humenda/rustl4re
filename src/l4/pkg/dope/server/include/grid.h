/*
 * \brief   Interface of DOpE Grid widget module
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

#ifndef _DOPE_GRID_H_
#define _DOPE_GRID_H_

#include "widget.h"

struct grid_methods;
struct grid_data;

#define GRID struct grid

#define GRID_STICKY_EAST  0x01
#define GRID_STICKY_WEST  0x02
#define GRID_STICKY_NORTH 0x04
#define GRID_STICKY_SOUTH 0x08

struct grid {
	struct widget_methods *gen;
	struct grid_methods   *grid;
	struct widget_data    *wd;
	struct grid_data      *gd;
};

struct grid_methods {
	void (*add)     (GRID *,WIDGETARG *new_child);
	void (*remove)  (GRID *,WIDGETARG *child);
	void (*set_row) (GRID *,WIDGETARG *child,s32 row_idx);
	void (*set_col) (GRID *,WIDGETARG *child,s32 col_idx);
};

struct grid_services {
	GRID *(*create) (void);
};


#endif /* _DOPE_GRID_H_ */

