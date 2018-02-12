/*
 * \brief   Interface of DOpE Background widget module
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

#ifndef _DOPE_BACKGROUND_H_
#define _DOPE_BACKGROUND_H_

struct background_methods;
struct background_data;

#define BACKGROUND struct background

#define BG_STYLE_WIN 0
#define BG_STYLE_DESK 1

struct background {
	struct widget_methods       *gen;
	struct background_methods   *bg;
	struct widget_data          *wd;
	struct background_data      *bd;
};

struct background_methods {
	void (*set_style) (BACKGROUND *,long style);
	void (*set_click) (BACKGROUND *,void (*click)(void *));
};

struct background_services {
	BACKGROUND *(*create) (void);
};


#endif /* _DOPE_BACKGROUND_H_ */

