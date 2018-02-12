/*
 * \brief   Interface of DOpE VScreen widget module
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

#ifndef _DOPE_VSCREEN_H_
#define _DOPE_VSCREEN_H_

#include "widget.h"

struct vscreen_methods;
struct vscreen_data;

#define VSCREEN struct vscreen

#include "gfx.h"

struct vscreen {
	struct widget_methods  *gen;
	struct vscreen_methods *vscr;
	struct widget_data     *wd;
	struct vscreen_data    *vd;
};

struct vscreen_methods {
	void (*reg_server) (VSCREEN *, char *server_ident);
	void (*waitsync)   (VSCREEN *);
	void (*refresh)    (VSCREEN *, s32 x, s32 y, s32 w, s32 h);
	GFX_CONTAINER *(*get_image) (VSCREEN *);
};

struct vscreen_services {
	VSCREEN *(*create) (void);
};

#endif /* _DOPE_VSCREEN_H_ */

