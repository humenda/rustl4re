/*
 * \brief   Interface of DOpE VTextScreen widget module
 * \date    2004-03-02
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

#ifndef _DOPE_VTEXTSCREEN_H_
#define _DOPE_VTEXTSCREEN_H_

#include "widget.h"

struct vtextscreen_methods;
struct vtextscreen_data;

#define VTEXTSCREEN struct vtextscreen

#include "gfx.h"

struct vtextscreen {
	struct widget_methods      *gen;
	struct vtextscreen_methods *vts;
	struct widget_data         *wd;
	struct vtextscreen_data    *vd;
};

struct vtextscreen_methods {
	s32   (*probe_mode)    (VTEXTSCREEN *, s32 width, s32 height, char *mode);
	s32   (*set_mode)      (VTEXTSCREEN *, s32 width, s32 height, char *mode);
	char *(*map)           (VTEXTSCREEN *, char *dst_thread_ident);
	void  (*refresh)       (VTEXTSCREEN *, s32 x, s32 y, s32 w, s32 h);
	GFX_CONTAINER *(*get_image) (VTEXTSCREEN *);
};

struct vtextscreen_services {
	VTEXTSCREEN *(*create) (void);
};

#endif /* _DOPE_VTEXTSCREEN_H_ */

