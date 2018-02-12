/*
 * \brief   Interface of DOpE Scrollbar widget module
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

#ifndef _DOPE_SCROLLBAR_H_
#define _DOPE_SCROLLBAR_H_

#include "widget.h"

struct scrollbar_methods;
struct scrollbar_data;

#define SCROLLBAR struct scrollbar

struct scrollbar {
	struct widget_methods    *gen;
	struct scrollbar_methods *scroll;
	struct widget_data       *wd;
	struct scrollbar_data    *sd;
};

struct scrollbar_methods {
	void  (*set_orient)        (SCROLLBAR *, char *orient);
	char *(*get_orient)        (SCROLLBAR *);
	void  (*set_autoview)      (SCROLLBAR *, long av);
	long  (*get_autoview)      (SCROLLBAR *);
	void  (*set_slider_x)      (SCROLLBAR *, s32 new_sx);
	u32   (*get_slider_x)      (SCROLLBAR *);
	void  (*set_slider_y)      (SCROLLBAR *, s32 new_sy);
	u32   (*get_slider_y)      (SCROLLBAR *);
	void  (*set_real_size)     (SCROLLBAR *, u32 new_real_size);
	u32   (*get_real_size)     (SCROLLBAR *);
	void  (*set_view_size)     (SCROLLBAR *, u32 new_view_size);
	u32   (*get_view_size)     (SCROLLBAR *);
	void  (*set_view_offset)   (SCROLLBAR *, s32 new_view_offset);
	u32   (*get_view_offset)   (SCROLLBAR *);
	s32   (*get_arrow_size)    (SCROLLBAR *);
	void  (*reg_scroll_update) (SCROLLBAR *, void (*callback)(void *), void *arg);
};

struct scrollbar_services {
	SCROLLBAR *(*create) (void);
};


#endif /* _DOPE_SCROLLBAR_H_ */

