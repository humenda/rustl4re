/*!
 * \file	init.h
 * \brief	init stuff
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef __INIT_H_
#define __INIT_H_

#include <l4/sys/l4int.h>
#include "vidix.h"

struct l4con_vc;

typedef struct
{
  void (*copy)(struct l4con_vc*, 
	       int sx, int sy, int width, int height, int dx, int dy);
  void (*fill)(struct l4con_vc*,
	       int sx, int sy, int width, int height, unsigned color);
  void (*blit)(struct l4con_vc*,
               const l4_uint8_t* chardata, int width, int height,
	       int yy, int xx);
  void (*sync)(void);
  void (*pan) (int *x, int *y);
  void (*drty)(int sx, int sy, int width, int height);

  int  (*cscs_init) (vidix_playback_t *config);
  void (*cscs_start)(void);
  void (*cscs_stop) (void);
  void (*cscs_grkey)(const vidix_grkey_t *grkey);
  void (*cscs_eq)   (const vidix_video_eq_t *eq);

  unsigned int caps;
} con_accel_t;

int  con_hw_init(unsigned short xres, unsigned short yres, unsigned char *bits,
                 unsigned short bpl,
		 l4_addr_t vid_mem_addr, l4_size_t vid_mem_size,
     		 con_accel_t *accel, l4_uint8_t **map_vid_mem_addr);

extern l4_addr_t      hw_vid_mem_addr, hw_vid_mem_size;
extern l4_addr_t      hw_map_vid_mem_addr;
extern unsigned short hw_xres, hw_yres;
extern unsigned char  hw_bits;
extern unsigned short hw_bpl;

#define ACCEL_FAST_COPY		0x00000001
#define ACCEL_FAST_FILL		0x00000002
#define ACCEL_FAST_CSCS_YV12	0x00000004
#define ACCEL_FAST_CSCS_YUY2	0x00000008
#define ACCEL_FAST_CSCS		(ACCEL_FAST_CSCS_YV12|ACCEL_FAST_CSCS_YUY2)
#define ACCEL_COLOR_KEY		0x00000010
#define ACCEL_EQ_BRIGHTNESS	0x00000020
#define ACCEL_EQ_CONTRAST	0x00000040
#define ACCEL_EQ_SATURATION	0x00000080
#define ACCEL_EQUALIZER		(ACCEL_EQ_BRIGHTNESS|ACCEL_EQ_CONTRAST|\
				 ACCEL_EQ_SATURATION)
#define ACCEL_POST_DIRTY	0x00000100

#endif
