/*
 * \brief   Interface of DOpE font conversion module
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

#ifndef _DOPE_FONTCONV_H_
#define _DOPE_FONTCONV_H_

struct fontconv_services {
	s16  (*probe)               (void *fontadr);
	void (*gen_width_table)     (void *fontadr,s32 *dst_wtab);
	void (*gen_offset_table)    (void *fontadr,s32 *dst_otab);
	u8  *(*get_name)            (void *fontadr);
	u16  (*get_top)             (void *fontadr);
	u16  (*get_bottom)          (void *fontadr);
	u32  (*get_image_width)     (void *fontadr);
	u32  (*get_image_height)    (void *fontadr);
	void (*gen_image)           (void *fontadr,u8 *dst_img);
};


#endif /* _DOPE_FONTCONV_H_ */

