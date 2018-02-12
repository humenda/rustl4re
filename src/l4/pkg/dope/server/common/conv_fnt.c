/*
 * \brief   DOpE Button widget module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module converts fnt data to a generic font
 * structure that can be  used by other components
 * of DOpE such as the font manager module.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "fontconv.h"

/*** FILE HEADER STRUCTURE OF *.FNT FILES ***/
struct fntfile_hdr {
	u16   font_id;        /* font number                       */
	u16   point;          /* size                              */
	char  name[32];       /* font name                         */
	u16   first_ade;      /* first ascii char                  */
	u16   last_ade;       /* last ascii char                   */
	u16   top;            /* distance topline <-> baseline     */
	u16   ascent;         /* distance ascentline <-> baseline  */
	u16   half;           /* distance halfline <-> baseline    */
	u16   descent;        /* distance descentline <-> baseline */
	u16   bottom;         /* distance bottomline <-> baseline  */
	u16   max_char_width; /* biggest character width           */
	u16   max_cell_width; /* biggest character cell with       */
	u16   left_offset;    /* left offset for italic            */
	u16   right_offset;   /* right offset for italic           */
	u16   thicken;        /* scale value for bold              */
	u16   ul_size;        /* height of underscore              */
	u16   lighten;        /* mask for bright style             */
	u16   skew;           /* mask for italic                   */
	u16   flags;          /* fontflags                         */
	u32   hor_table;      /* pointer to horizontal table       */
	u32   off_table;      /* pointer to char offset table      */
	u32   dat_table;      /* pointer to font image             */
	u16   form_width;     /* width of font image               */
	u16   form_height;    /* height of font image              */
	u32   next_font;      /* unused                            */
};

int init_conv_fnt(struct dope_services *d);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** CONVERT INTEL 16BIT VALUE TO HOST FORMAT ***/
static u16 i2u16(u16 *src) {
	u8 *low  = ((u8 *)src)+1;
	u8 *high = (u8 *)src;
	return (((u16)(*low))<<8) + (u16)(*high);
}


/*** UTILITY: CONVERT INTEL 32BIT VALUE TO HOST FORMAT ***/
static u32 i2u32(u32 *src) {
	u8 *a = ((u8 *)src);
	u8 *b = ((u8 *)src) + 1;
	u8 *c = ((u8 *)src) + 2;
	u8 *d = ((u8 *)src) + 3;
	return ((u32)(*a))      + (((u32)(*b))<<8)
	    + (((u32)(*c))<<16) + (((u32)(*d))<<24);
}


/*** UTILITY: CONVERT MOTOROLA 16BIT VALUE TO HOST FORMAT ***/
static u16 m2u16(u16 *src) {
	u8 *low  = ((u8 *)src)+1;
	u8 *high =  (u8 *)src;
	return (((u16)(*high))<<8) + (u16)(*low);
}


/*** UTILITY: CONVERT MOTOROLA 32BIT VALUE TO HOST FORMAT ***/
static u32 m2u32(u32 *src) {
	u8 *a = ((u8 *)src);
	u8 *b = ((u8 *)src) + 1;
	u8 *c = ((u8 *)src) + 2;
	u8 *d = ((u8 *)src) + 3;
	return ((u32)(*d))      + (((u32)(*c))<<8)
	    + (((u32)(*b))<<16) + (((u32)(*a))<<24);
}


/*** UTILITY: CHECK IF FONT IS IN INTEL FORMAT ***
 *
 * \return  1 if the font is in intel format
 *          0 if the font is in motorla format
 */
static inline int intel_format(struct fntfile_hdr *fnt) {
	return !(fnt->flags & 4);
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** INITIALISES AND PROBES FONT ***/
static s16 font_probe(struct fntfile_hdr *fnt) {
	u32 max_char_w, img_w, img_h, point, first_ade, last_ade, top, bottom;
	u16 (*get_u16) (u16 *);
	INFO(char *dbg = "ConvertFNT(probe): ");

	if (intel_format(fnt)) {
		INFO(printf("%s font is in intel format\n", dbg));
		get_u16 = i2u16;
	} else  {
		INFO(printf("%s font is in motorola format\n", dbg));
		get_u16 = m2u16;
	}

	fnt->name[31] = 0;

	max_char_w = get_u16(&fnt->max_char_width);
	img_w      = get_u16(&fnt->form_width)*8;
	img_h      = get_u16(&fnt->form_height);
	point      = get_u16(&fnt->point);
	first_ade  = get_u16(&fnt->first_ade);
	last_ade   = get_u16(&fnt->last_ade);
	top        = get_u16(&fnt->top);
	bottom     = get_u16(&fnt->bottom);

	INFO(printf("%s image width:  %u\n",dbg,img_w));
	INFO(printf("%s image height: %u\n",dbg,img_h));
	INFO(printf("%s points:       %u\n",dbg,point));
	INFO(printf("%s first_ade:    %u\n",dbg,first_ade));
	INFO(printf("%s last_ade:     %u\n",dbg,last_ade));
	INFO(printf("%s top:          %u\n",dbg,top));
	INFO(printf("%s bottom:       %u\n",dbg,bottom));
	INFO(printf("%s name:         %s\n",dbg,fnt->name));

	/* strange values -> seems not to be a valid fnt */
	if (max_char_w > 100 || img_h < 1            || img_h > 100
	 || img_w < 1        || point > 50           || first_ade > 255
	 || last_ade > 255   || first_ade > last_ade || top > img_h
	 || bottom > img_h   || top < bottom) {

		INFO(printf("%sthis doesnt seem to be valid fnt data - sorry\n",dbg));
		return 0;
	}
	return 1;
}


/*** GENERATES WIDTH TABLE FOR THE SPECIFIED FONT ***
 *
 * The width table will be generated at the given destination
 * adress. It has one 32bit-entry per ASCII value so its size
 * is 256*4 bytes.
 */
static void font_gen_width_table(struct fntfile_hdr *fnt, s32 *dst_wtab) {
	u32   i;
	u16   first_ade, last_ade;
	u16  *offsets;
	s32   width;
	u16 (*get_u16) (u16 *);
	u32 (*get_u32) (u32 *);

	if (intel_format(fnt)) {
		get_u16 = i2u16;
		get_u32 = i2u32;
	} else {
		get_u16 = m2u16;
		get_u32 = m2u32;
	}

	first_ade = get_u16(&fnt->first_ade);
	last_ade  = get_u16(&fnt->last_ade);
	offsets = (u16 *)((get_u32((u32 *)(&fnt->off_table))) + (long)fnt);

	for (i = 0; i < first_ade; i++) *(dst_wtab++) = 0;
	for (i = first_ade; i < last_ade; i++) {
		width = get_u16(offsets + 1) - get_u16(offsets);
		offsets++;
		*(dst_wtab++) = width;
	}
	for (i = last_ade; i < 256; i++) *(dst_wtab++) = 0;
}


/*** GENERATES OFFSET TABLE FOR THE SPECIFIED FONT ***
 *
 * The offset table will be generated at the given destination
 * adress. Its size is 256*4 bytes.
 */
static void font_gen_offset_table(struct fntfile_hdr *fnt, s32 *dst_otab) {
	u16   first_ade, last_ade, i;
	u16  *offsets;
	u16 (*get_u16) (u16 *);
	u32 (*get_u32) (u32 *);

	if (intel_format(fnt)) {
		get_u16 = i2u16;
		get_u32 = i2u32;
	} else {
		get_u16 = m2u16;
		get_u32 = m2u32;
	}

	first_ade = get_u16(&fnt->first_ade);
	last_ade  = get_u16(&fnt->last_ade);
	offsets   = (u16 *)((get_u32((u32 *)(&fnt->off_table))) + (long)fnt);

	for (i = 0; i < first_ade; i++) *(dst_otab++) = 0;
	for (i = first_ade; i < last_ade + 1; i++) {
		*(dst_otab++) = get_u16(offsets++);
	}
	for (i = last_ade + 1; i < 256; i++) *(dst_otab++) = 0;
}


/*** RETURNS NAME OF THE FONT ***/
static char *font_get_name(struct fntfile_hdr *fnt) {
	fnt->name[31] = 0;
	return (char *)&fnt->name;
}


/*** GET TOP LINE OF FONT ***/
static u16 font_get_top(struct fntfile_hdr *fnt) {

	if (intel_format(fnt))
		return i2u16(&fnt->top);
	else
		return m2u16(&fnt->top);
}


/*** GET BOTTOM LINE OF FONT ***/
static u16 font_get_bottom(struct fntfile_hdr *fnt) {

	if (intel_format(fnt))
		return i2u16(&fnt->bottom);
	else
		return m2u16(&fnt->bottom);
}


/*** GET HEIGHT OF FONT IMAGE ***/
static u32 font_get_image_width(struct fntfile_hdr *fnt) {

	if (intel_format(fnt))
		return 8*i2u16(&fnt->form_width);
	else
		return 8*m2u16(&fnt->form_width);
}


/*** GET HEIGHT OF FONT IMAGE ***/
static u32 font_get_image_height(struct fntfile_hdr *fnt) {

	if (intel_format(fnt))
		return i2u16(&fnt->form_height);
	else
		return m2u16(&fnt->form_height);
}


/*** GENERATES CHUNKY-ORGANIZED FONT IMAGE ***
 *
 * The image will be generated at the specified
 * destination address such that the caller of
 * this routine has to take care about memory
 * allocation. The size of a font image is
 * image_width*image_height.
 */
static void font_gen_image(struct fntfile_hdr *fnt, u8 *dst) {
	s32  linelength, height;
	s32  i, j, k;
	u8   curr_byte;
	u8  *src;
	u16 (*get_u16) (u16 *);
	u32 (*get_u32) (u32 *);

	if (intel_format(fnt)) {
		get_u16 = i2u16;
		get_u32 = i2u32;
	} else {
		get_u16 = m2u16;
		get_u32 = m2u32;
	}
	linelength = get_u16(&fnt->form_width);
	height     = get_u16(&fnt->form_height);
	src = (u8 *)((get_u32((u32 *)(&fnt->dat_table))) + (long)fnt);

	for (j = 0; j < height; j++) {
		for (i = 0; i < linelength; i++) {
			curr_byte = *(src++);
			for (k = 0; k < 8; k++) {
				*(dst++)  = (((long)curr_byte)&0x0080) ? 255 : 0;
				curr_byte = curr_byte<<1;
			}
		}
	}
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct fontconv_services services = {
	(s16  (*) (void *))       font_probe,
	(void (*) (void *,s32 *)) font_gen_width_table,
	(void (*) (void *,s32 *)) font_gen_offset_table,
	(u8  *(*) (void *))       font_get_name,
	(u16  (*) (void *))       font_get_top,
	(u16  (*) (void *))       font_get_bottom,
	(u32  (*) (void *))       font_get_image_width,
	(u32  (*) (void *))       font_get_image_height,
	(void (*) (void *,u8 *))  font_gen_image,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_conv_fnt(struct dope_services *d) {
	d->register_module("ConvertFNT 1.0", &services);
	return 1;
}
