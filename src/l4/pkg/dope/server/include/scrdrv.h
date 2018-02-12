/*
 * \brief   Interface of DOpE screen driver
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

#ifndef _DOPE_SCRDRV_H_
#define _DOPE_SCRDRV_H_

struct scrdrv_services {
	long  (*set_screen)     (long width, long height, long depth);
	void  (*restore_screen) (void);
	long  (*get_scr_width)  (void);
	long  (*get_scr_height) (void);
	long  (*get_scr_depth)  (void);
	void *(*get_scr_adr)    (void);
	void *(*get_buf_adr)    (void);
	void  (*update_area)    (long x1, long y1, long x2, long y2);
	void  (*set_mouse_pos)  (long x,long y);
	void  (*set_mouse_shape)(void *);
//  void  (*mouse_off)      (void);
//  void  (*mouse_on)       (void);
//  void  (*set_draw_area)  (long x1, long y1, long x2, long y2);
};

#endif /* _DOPE_SCRDRV_H_ */
