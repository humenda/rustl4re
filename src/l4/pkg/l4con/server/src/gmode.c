/**
 * \file	con/server/src/gmode.c
 * \brief	graphics mode initialization
 *
 * \date	2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "gmode.h"

l4_uint16_t YRES_CLIENT;
l4_uint8_t  FONT_XRES, FONT_YRES;

l4_umword_t accel_caps  = 0;

int         panned;		/**< Display already panned? */
l4_uint32_t pan_offs_x;		/**< x offset for panning */
l4_uint32_t pan_offs_y;		/**< y offset for panning */

l4re_video_view_info_t fb_info;
l4_uint8_t  *gr_vmem;		/**< Linear video framebuffer. */
l4_uint8_t  *gr_vmem_maxmap;    /**< don't map fb beyond this address. */
l4_size_t    gr_vmem_size;      /**< Size of video framebuffer. */
l4_uint8_t  *vis_vmem;		/**< Visible video framebuffer. */
l4_addr_t    vis_offs;		/**< vis_vmem - gr_vmem. */
