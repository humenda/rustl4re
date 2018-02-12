/*
 * \brief   DOpE font manager module
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

#ifndef _DOPE_FONTMAN_H_
#define _DOPE_FONTMAN_H_

struct font {
	s32  font_id;
	s32 *width_table;
	s32 *offset_table;
	s32  img_w,img_h;
	s16  top,bottom;
	u8  *image;
	u8  *name;
};


struct fontman_services {
	struct font *(*get_by_id)       (s32 font_id);
	s32          (*calc_str_width)  (s32 font_id, char *str);
	s32          (*calc_str_height) (s32 font_id, char *str);
	s32          (*calc_char_idx)   (s32 font_id, char *str, s32 pixpos);
};


#endif /* _DOPE_FONTMAN_H_ */
