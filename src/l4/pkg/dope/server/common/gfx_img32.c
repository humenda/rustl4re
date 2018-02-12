/*
 * \brief   DOpE gfx 32bit image handler module
 * \date    2003-04-07
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

#include "dopestd.h"
#include "sharedmem.h"
#include "gfx_handler.h"
#include "gfx.h"

static struct sharedmem_services *shmem;

struct gfx_ds_data {
	s32 w,h;            /* width and height of the image */
	SHAREDMEM *smb;     /* shared memory block */
	u32 *pixels;        /* 32bit color values of the pixels */
};

int init_gfximg32(struct dope_services *d);

/*****************************
 *** GFX HANDLER FUNCTIONS ***
 *****************************/

static s32 img_get_width(struct gfx_ds_data *img) {
	return img->w;
}

static s32 img_get_height(struct gfx_ds_data *img) {
	return img->h;
}

static s32 img_get_type(struct gfx_ds_data *img) {
  (void)img;
	return GFX_IMG_TYPE_RGBA32;
}

static void img_destroy(struct gfx_ds_data *img) {
	free(img);
}

static void *img_map(struct gfx_ds_data *img) {
	return img->pixels;
}

static s32 img_share(struct gfx_ds_data *img, THREAD *dst_thread) {
	return shmem->share(img->smb, dst_thread);
}

static s32 img_get_ident(struct gfx_ds_data *img, char *dst_ident) {
	shmem->get_ident(img->smb, dst_ident);
	return 0;
}

/*************************
 *** SERVICE FUNCTIONS ***
 *************************/


static struct gfx_ds_data *create(s32 width, s32 height, struct gfx_ds_handler **handler) {
  (void)handler;
	struct gfx_ds_data *new;
	new = zalloc(sizeof(struct gfx_ds_data) + width*height*4);
	if (!new) return NULL;
	new->w = width;
	new->h = height;
	new->smb = shmem->alloc(width*height*4);
	new->pixels = (u32 *)(shmem->get_address(new->smb));
	if (new->pixels) memset(new->pixels, 0, width*height*4);
	return new;
}

static s32 register_gfx_handler(struct gfx_ds_handler *handler) {
	handler->get_width  = img_get_width;
	handler->get_height = img_get_height;
	handler->get_type   = img_get_type;
	handler->destroy    = img_destroy;
	handler->map        = img_map;
	handler->share      = img_share;
	handler->get_ident  = img_get_ident;
	return 0;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct gfx_handler_services services = {
	create,
	register_gfx_handler,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_gfximg32(struct dope_services *d) {
	shmem = d->get_module("SharedMemory 1.0");
	d->register_module("GfxImage32 1.0",&services);
	return 1;
}
