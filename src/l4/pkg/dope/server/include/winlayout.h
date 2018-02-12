/*
 * \brief   Interface of the window layout module of DOpE
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

#ifndef _DOPE_WINLAYOUT_H_
#define _DOPE_WINLAYOUT_H_

#include "widget.h"

struct winlayout_services {
	WIDGET *(*create_win_elements) (s32 elem_mask, int x, int y, int width, int height);
	void    (*resize_win_elements) (WIDGET *first_elem, s32 elem_mask,
	                                int x, int y, int width, int height);
	void    (*set_win_title)       (WIDGET *first_elem,char *new_title);
	char   *(*get_win_title)       (WIDGET *first_elem);
	void    (*set_win_state)       (WIDGET *first_elem,s32 state);
	s32     (*get_left_border)     (s32 elem_mask);
	s32     (*get_right_border)    (s32 elem_mask);
	s32     (*get_top_border)      (s32 elem_mask);
	s32     (*get_bottom_border)   (s32 elem_mask);
};

#endif /* _DOPE_WINLAYOUT_H_ */
