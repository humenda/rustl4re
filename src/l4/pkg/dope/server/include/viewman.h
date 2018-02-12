/*
 * \brief   Interface of view manager module
 * \date    2004-09-03
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

#ifndef _DOPE_VIEWMAN_H_
#define _DOPE_VIEWMAN_H_

struct view;

struct viewman_services {
	struct view *(*create)    (void);
	void         (*destroy)   (struct view *);
	int          (*place)     (struct view *, int x, int y, int w, int h);
	int          (*top)       (struct view *);
	int          (*back)      (struct view *);
	int          (*set_title) (struct view *, const char *title);
	int          (*set_bg)    (struct view *);
};


#endif /* _DOPE_VIEWMAN_H_ */
