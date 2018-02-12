/*!
 * \file	vidix.h
 * \brief	backend scaler stuff
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

#ifndef __VIDIX_H_
#define __VIDIX_H_

/* taken from mplayer/vidix/vidix.h */

typedef struct
{
  unsigned y,u,v;
} vidix_yuv_t;

typedef struct
{
  unsigned x,y,w,h;
  vidix_yuv_t pitch;
} vidix_rect_t;

typedef struct
{
#define CKEY_FALSE	0
#define CKEY_TRUE	1
#define CKEY_EQ		2
#define CKEY_NEQ	3
  unsigned	op;		/* defines logical operation */
  unsigned char	red;
  unsigned char	green;
  unsigned char	blue;
  unsigned char	reserved;
} vidix_ckey_t;

typedef struct
{
#define VKEY_FALSE	0
#define VKEY_TRUE	1
#define VKEY_EQ		2
#define VKEY_NEQ	3
  unsigned	op;		/* defines logical operation */
  unsigned char	key[8];
} vidix_vkey_t;

typedef struct
{
  vidix_ckey_t	ckey;		/* app -> driver: color key */
  vidix_vkey_t	vkey;		/* app -> driver: video key */
#define KEYS_PUT	0
#define KEYS_AND	1
#define KEYS_OR		2
#define KEYS_XOR	3
  unsigned	key_op;		/* app -> driver: keys operations */
} vidix_grkey_t;

typedef struct
{
  unsigned	fourcc;		/* app -> driver: movies's fourcc */
  unsigned	capability;	/* app -> driver: what capability to use */
  unsigned	blend_factor;	/* app -> driver: blending factor */
  vidix_rect_t	src;            /* app -> driver: original movie size */
  vidix_rect_t	dest;           /* app -> driver: dest movie size. 
				 * driver->app dest_pitch */
#define VID_PLAY_INTERLEAVED_UV	0x00000001
  				/* driver -> app: interleaved UV planes */
#define INTERLEAVING_UV		0x00001000
				/* UVUVUVUVUV used by Matrox G200 */
#define INTERLEAVING_VU		0x00001001
				/* VUVUVUVUVU */
  int		flags;

  /* memory model */
  unsigned	frame_size;	/* driver -> app: destinition frame size */
  unsigned	num_frames;	/* app -> driver: after call: driver -> app */
#define VID_PLAY_MAXFRAMES 64	/* reasonable limitation for decoding ahead */
  unsigned	offsets[VID_PLAY_MAXFRAMES];	/* driver -> app */
  vidix_yuv_t	offset;		/* driver -> app: relative offs within
				 * frame for yuv planes */
  void		*dga_addr;
} vidix_playback_t;

typedef struct
{
#define VEQ_CAP_NONE			0x00000000UL
#define VEQ_CAP_BRIGHTNESS		0x00000001UL
#define VEQ_CAP_CONTRAST		0x00000002UL
#define VEQ_CAP_SATURATION		0x00000004UL
#define VEQ_CAP_HUE			0x00000008UL
#define VEQ_CAP_RGB_INTENSITY		0x00000010UL
	int		cap;		/* on get_eq should contain capability
					   of equalizer on set_eq should 
					   contain using fields */
/* end-user app can have presets like: cold-normal-hot picture and so on */
	int		brightness;	/* -1000 : +1000 */
	int		contrast;	/* -1000 : +1000 */
	int		saturation;	/* -1000 : +1000 */
	int		hue;		/* -1000 : +1000 */
	int		red_intensity;	/* -1000 : +1000 */
	int		green_intensity;/* -1000 : +1000 */
	int		blue_intensity; /* -1000 : +1000 */
} vidix_video_eq_t;

#endif

