/*!
 * \file	savage_vid.c
 * \brief	Hardware Acceleration for S3 Savage cards (backend scaler)
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

/* Programming of backend scaler for "new" savage chips
 * (Savage/MX{-MV}, Savage/IX{-MV}, SuperSavage, Savage 2000) */

#include <l4/sys/types.h>

#include "init.h"
#include "savage.h"
#include "savage_regs.h"
#include "vidix.h"
#include "fourcc.h"

/* Stream Processor 1 */

#define PRI_STREAM_FBUF_ADDR0           0x81c0
#define PRI_STREAM_FBUF_ADDR1           0x81c4
#define PRI_STREAM_STRIDE               0x81c8
#define PRI_STREAM_BUFFERSIZE           0x8214

#define SEC_STREAM_CKEY_LOW             0x8184
#define SEC_STREAM_CKEY_UPPER           0x8194
#define BLEND_CONTROL                   0x8190
#define SEC_STREAM_COLOR_CONVERT1       0x8198
#define SEC_STREAM_COLOR_CONVERT2       0x819c
#define SEC_STREAM_COLOR_CONVERT3       0x81e4
#define SEC_STREAM_HSCALING             0x81a0
#define SEC_STREAM_BUFFERSIZE           0x81a8
#define SEC_STREAM_HSCALE_NORMALIZE	0x81ac
#define SEC_STREAM_VSCALING             0x81e8
#define SEC_STREAM_FBUF_ADDR0           0x81d0
#define SEC_STREAM_FBUF_ADDR1           0x81d4
#define SEC_STREAM_FBUF_ADDR2           0x81ec
#define SEC_STREAM_STRIDE               0x81d8
#define SEC_STREAM_WINDOW_START         0x81f8
#define SEC_STREAM_WINDOW_SZ            0x81fc
#define SEC_STREAM_TILE_OFF             0x821c
#define SEC_STREAM_OPAQUE_OVERLAY       0x81dc


/* Stream Processor 2 */

#define PRI_STREAM2_FBUF_ADDR0          0x81b0
#define PRI_STREAM2_FBUF_ADDR1          0x81b4
#define PRI_STREAM2_STRIDE              0x81b8
#define PRI_STREAM2_BUFFERSIZE          0x8218

#define SEC_STREAM2_CKEY_LOW            0x8188
#define SEC_STREAM2_CKEY_UPPER          0x818c
#define SEC_STREAM2_HSCALING            0x81a4
#define SEC_STREAM2_VSCALING            0x8204
#define SEC_STREAM2_BUFFERSIZE          0x81ac
#define SEC_STREAM2_FBUF_ADDR0          0x81bc
#define SEC_STREAM2_FBUF_ADDR1          0x81e0
#define SEC_STREAM2_FBUF_ADDR2          0x8208
#define SEC_STREAM2_STRIDE_LPB          0x81cc
#define SEC_STREAM2_COLOR_CONVERT1      0x81f0
#define SEC_STREAM2_COLOR_CONVERT2      0x81f4
#define SEC_STREAM2_COLOR_CONVERT3      0x8200
#define SEC_STREAM2_WINDOW_START        0x820c
#define SEC_STREAM2_WINDOW_SZ           0x8210
#define SEC_STREAM2_OPAQUE_OVERLAY      0x8180

static unsigned int savage_blendBase;
static int ck_lo, ck_hi, ck_active = 0;

static void
savage_set_colorkey(void)
{
  if (!ck_active)
    {
      /* disable colorkey */
      savage_out32(SEC_STREAM_CKEY_LOW, 0);
      savage_out32(SEC_STREAM_CKEY_UPPER, 0);
    }
  else
    {
      savage_out32(SEC_STREAM_CKEY_LOW, ck_lo);
      savage_out32(SEC_STREAM_CKEY_UPPER, ck_hi);
    }

  savage_out32(BLEND_CONTROL, savage_blendBase);
}

static void
savage_set_grkey(const vidix_grkey_t *grkey)
{
  if (grkey->ckey.op == CKEY_TRUE)
    {
      switch (hw_bits)
	{
	case 15:
	  ck_lo = ck_hi = ((grkey->ckey.red   & 0xF8)<<16)
			| ((grkey->ckey.green & 0xF8)<< 8)
			| ((grkey->ckey.blue  & 0xF8)    );
	  ck_lo |= 0x45000000;
	  ck_hi |= 0x45000000;
	  break;
	case 16:
	  ck_lo = ck_hi = ((grkey->ckey.red   & 0xF8)<<16)
			| ((grkey->ckey.green & 0xFC)<< 8)
			| ((grkey->ckey.blue  & 0xF8)    );
	  ck_lo |= 0x46000000;
	  ck_hi |= 0x46020002;
	  break;
	case 24:
	default:
	  ck_lo = ck_hi = ((grkey->ckey.red   & 0xFF)<<16)
			| ((grkey->ckey.green & 0xFF)<< 8)
			| ((grkey->ckey.blue  & 0xFF)    );
	  ck_lo |= 0x47000000;
	  ck_hi |= 0x47000000;
	  break;
	}
      ck_active = 1;
      savage_set_colorkey();
    }
  else
    ck_active = 0;
}

#define EXT_MISC_CTRL2              0x67
#define ENABLE_STREAM1              0x04

static unsigned int
savage_get_blend_fourcc(unsigned int fourcc)
{
  switch (fourcc)
    {
    case IMGFMT_YUY2:	/* packed */
    case IMGFMT_YV12:	/* planar: translate into YUY2 */
    case IMGFMT_I420:	/* planar: translate into YUY2 */
      return 1;
    case IMGFMT_Y211:	/* packed */
      return 4;
    case IMGFMT_RGB15:	/* packed */
      return 3;
    case IMGFMT_RGB16:	/* packed */
      return 5;
    default:
      return 0;
    }
}

static void
savage_streams_on(vidix_playback_t *config)
{
  /* Unlock extended registers. */
  vga_out16(0x3d4, 0x4838);
  vga_out16(0x3d4, 0xa039);
  vga_out16(0x3c4, 0x0608);

  vga_out8(0x3d4, EXT_MISC_CTRL2);

  if (S3_SAVAGEMOB_SERIES(savage_chip)
      || (savage_chip == S3_SUPERSAVAGE)
      || (savage_chip == S3_SAVAGE2000))
    {
      unsigned char jStreamsControl = vga_in8(0x3d5) | ENABLE_STREAM1;

      /* Wait for VBLANK. */
      savage_vertical_retrace_wait();

      /* Fire up streams! */
      vga_out16(0x3d4, (jStreamsControl << 8) | EXT_MISC_CTRL2);

      savage_blendBase = savage_get_blend_fourcc(config->fourcc)<<9;
      savage_out32(BLEND_CONTROL, savage_blendBase);

      /* These values specify brightness, contrast, saturation and hue. */
      savage_out32(SEC_STREAM_COLOR_CONVERT1, 0x0000C892);
      savage_out32(SEC_STREAM_COLOR_CONVERT2, 0x00039F9A);
      savage_out32(SEC_STREAM_COLOR_CONVERT3, 0x01F1547E);
    }
  else
    {
      /* old engine not supported */
    }

  /* Wait for VBLANK. */ 
  savage_vertical_retrace_wait();
}

static void
savage_compute_framesize(vidix_playback_t *info)
{
  unsigned awidth;

  switch(info->fourcc)
    {
    case IMGFMT_I420: /* translate into YUY2 */
    case IMGFMT_YV12: /* translate into YV12 */
    case IMGFMT_YUY2:
      awidth = ((info->src.w<<1)+15)&~15;
      info->frame_size = awidth*info->src.h;
      break;
    case IMGFMT_Y211:
      awidth = (((info->src.w<<1)+15)&~15)/2;
      info->frame_size = awidth*info->src.h;
      break;
    }

  // align to 1MB boundary */
  info->frame_size = (info->frame_size + 1024*1024 - 1) & ~(1024*1024-1);
}

static int
savage_init_video_new(vidix_playback_t *config)
{
  int src_h  = config->src.h;
  int src_w  = config->src.w;
  int drw_w  = config->dest.w;
  int drw_h  = config->dest.h;
  int pitch, offset;

  pitch = ((src_w<<1)+15) & ~15;

  savage_compute_framesize(config);

  config->offsets[0] = 0;
  config->offset.y = config->offset.u = config->offset.v = 0;

  if (   config->fourcc == IMGFMT_IYUV
      || config->fourcc == IMGFMT_YV12
      || config->fourcc == IMGFMT_I420)
    {
      config->offset.y = 0;
      config->offset.u = (pitch*src_h + 15)&~15;
      config->offset.v = (config->offset.u + (pitch*src_h>>2) + 15)&~15;

      if (config->fourcc == IMGFMT_I420 || config->fourcc == IMGFMT_IYUV)
	{
#if 0
	  l4_uint32_t tmp;
	  tmp = config->offset.u;
	  config->offset.u = config->offset.v;
	  config->offset.v = tmp;
#endif
	}
    }
  else if (config->fourcc == IMGFMT_YVU9)
    {
      config->offset.y = 0;
      config->offset.u = (pitch*src_h + 15)&~15;
      config->offset.v = (config->offset.u + (pitch*src_h>>4) + 15)&~15;
    }

  offset =
    (hw_vid_mem_size - config->frame_size * config->num_frames) & 0xfff00000;
  config->dga_addr = (char *)hw_map_vid_mem_addr + offset;

  /* Calculate horizontal and vertical scale factors. */
  if (savage_chip == S3_SAVAGE2000)
    {
      savage_out32(SEC_STREAM_HSCALING, (65536 * src_w / drw_w) & 0x1FFFFF );
      if (src_w < drw_w)
	savage_out32(SEC_STREAM_HSCALE_NORMALIZE,
		     ((2048 * src_w / drw_w) & 0x7ff) << 16);
      else
	savage_out32(SEC_STREAM_HSCALE_NORMALIZE, 2048 << 16);
      savage_out32(SEC_STREAM_VSCALING, (65536 * src_h / drw_h) & 0x1FFFFF);
    }
  else
    {
      savage_out32(SEC_STREAM_HSCALING,
	  ((src_w & 0xfff)<<20) | ((65536 * src_w / drw_w) & 0x1FFFF));
      /* XXX need to add 00040000 if src stride > 2048 */
      savage_out32(SEC_STREAM_VSCALING, 
	  ((src_h & 0xfff)<<20) | ((65536 * src_h / drw_h) & 0x1FFFF));
    }

  /* Set surface location and stride.  We use x1>>15 because all surfaces
   * are 2 bytes/pixel. */
  savage_out32(SEC_STREAM_FBUF_ADDR0, 
      (offset+(config->src.x>>15)) & 0x7ffff0);
  savage_out32(SEC_STREAM_STRIDE, pitch & 0xfff);
  savage_out32(SEC_STREAM_WINDOW_START, 
      ((config->dest.x+1)<<16) | (config->dest.y+1));
  savage_out32(SEC_STREAM_WINDOW_SZ, ((drw_w) << 16) | drw_h);

  /* Set color key on primary. */
  savage_set_colorkey();

  /* Set FIFO L2 on second stream. */
    {
      pitch = (pitch+7)/8 - 4;
      vga_out8(0x3d4, 0x92);
      vga_out8(0x3d5, (vga_in8(0x3d5) & 0x40) | (pitch >> 8) | 0x80);
      vga_out8(0x3d4, 0x93);
      vga_out8(0x3d5, pitch);
    }

  return 0;
}

static void
savage_start_video(void)
{
  savage_blendBase |= 0x08;
  savage_out32(BLEND_CONTROL, savage_blendBase);
}

static void
savage_stop_video(void)
{
  savage_blendBase &= ~0x08;
  savage_out32(BLEND_CONTROL, savage_blendBase);
}

static int
savage_init_video(vidix_playback_t *config)
{
  savage_streams_on(config);
  savage_init_video_new(config);

  return 0;
}

void
savage_vid_init(con_accel_t *accel)
{
  if (S3_SAVAGEMOB_SERIES(savage_chip)
      || (savage_chip == S3_SUPERSAVAGE)
      || (savage_chip == S3_SAVAGE2000))
    {
      accel->cscs_init  = savage_init_video;
      accel->cscs_start = savage_start_video;
      accel->cscs_stop  = savage_stop_video;
      accel->cscs_grkey = savage_set_grkey;
      accel->caps |= ACCEL_FAST_CSCS_YUY2 | ACCEL_COLOR_KEY;
    }
  /* old Savage chips not supported */
}

