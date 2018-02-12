/**
 * \file	con/server/src/gmode.h
 * \brief	Graphics mode initialization
 *
 * \date	2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _GMODE_H
#define _GMODE_H

#include <l4/sys/l4int.h>
#include <l4/re/c/video/goos.h>

/** initialize graphics mode. */
void init_gmode(void);

extern l4_uint8_t* gr_vmem;
extern l4_uint8_t* gr_vmem_maxmap;
extern l4_size_t   gr_vmem_size;
extern l4_uint8_t* vis_vmem;            /**< vsbl. mem (>gr_vmem if panned) */
extern l4_addr_t   vis_offs;            /**< vis_vmem - gr_vmem */

extern l4re_video_view_info_t view_info; /**< Framebuffer information */
extern l4_uint16_t YRES_CLIENT;         /**< pixels per row for clients */
extern l4_uint8_t  FONT_XRES;           /**< x-pixels per font character */
extern l4_uint8_t  FONT_YRES;           /**< y-pixels per font character */
extern int         panned;              /**< display is panned */
extern l4_umword_t accel_caps;
extern l4_uint32_t pan_offs_x;          /**< panned to position x */
extern l4_uint32_t pan_offs_y;          /**< panned to position y */

#endif /* !_GMODE_H */
