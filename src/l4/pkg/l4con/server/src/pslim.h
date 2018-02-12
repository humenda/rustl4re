/**
 * \file	l4con/server/src/pslim.h
 * \brief	pseudoSLIM functions
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2001-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _pSLIM_H
#define _pSLIM_H

#include <l4/l4con/l4con_pslim.h>

#include "l4con.h"

extern
void pslim_fill(struct l4con_vc*, int from_user, 
		l4con_pslim_rect_t *rect, l4con_pslim_color_t color);

extern
void pslim_bmap(struct l4con_vc*, int from_user, l4con_pslim_rect_t *rect,
		l4con_pslim_color_t fgc, l4con_pslim_color_t bgc, 
		void* bmap, l4_uint8_t mode);

extern
void pslim_set(struct l4con_vc*, int from_user, l4con_pslim_rect_t *rect,
	       void* pmap);

extern
void pslim_copy(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
		l4_int16_t dx, l4_int16_t dy);

extern
void pslim_cscs(struct l4con_vc *vc, int from_user, l4con_pslim_rect_t *rect,
		void* y, void* u, void* v, l4_uint8_t mode, l4_uint32_t scale);

extern void sw_copy(struct l4con_vc*, int, int, int, int, int, int);
extern void sw_fill(struct l4con_vc*, int, int, int, int, unsigned col);

#endif /* !_pSLIM_FUNC_H */
