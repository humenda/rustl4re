/*!
 * \file	ati_vid.c
 * \brief	Hardware Acceleration for ATI Mach64 cards (backend scaler)
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

/* most stuff taken from MPlayer/vidix driver */

#include <stdio.h>

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/sys/consts.h>

#include "init.h"
#include "ati.h"
#include "aty.h"
#include "fourcc.h"
#include "vidix.h"

#define VIDEO_KEY_FN_TRUE		0x001
#define VIDEO_KEY_FN_EQ			0x005
#define GRAPHIC_KEY_FN_TRUE		0x010
#define GRAPHIC_KEY_FN_EQ		0x050
#define CMP_MIX_AND			0x100

static int supports_planar;

typedef struct
{
  /* base address of yuv framebuffer */
  l4_uint32_t yuv_base;
  l4_uint32_t fourcc;
  /* YUV BES registers */
  l4_uint32_t reg_load_cntl;
  l4_uint32_t scale_inc;
  l4_uint32_t y_x_start;
  l4_uint32_t y_x_end;
  l4_uint32_t vid_buf_pitch;
  l4_uint32_t height_width;
  
  l4_uint32_t scale_cntl;
  l4_uint32_t exclusive_horz;
  l4_uint32_t auto_flip_cntl;
  l4_uint32_t filter_cntl;
  l4_uint32_t key_cntl;
  l4_uint32_t test;
  /* Configurable stuff */
  
  int brightness;
  int saturation;
  
  int ckey_on;
  l4_uint32_t graphics_key_clr;
  l4_uint32_t graphics_key_msk;
  
} bes_registers_t;

static l4_uint32_t mach64_overlay_offset = 0;
static bes_registers_t besr;
static l4_uint32_t mach64_buffer_base[10][3];
static int num_mach64_buffers=-1;

static void
ati_start_video(void)
{
  l4_uint32_t vf;
  wait_for_fifo(14);

  aty_st_le32(OVERLAY_Y_X_START,    besr.y_x_start);
  aty_st_le32(OVERLAY_Y_X_END,      besr.y_x_end);
  aty_st_le32(OVERLAY_SCALE_INC,    besr.scale_inc);
  aty_st_le32(SCALER_BUF_PITCH,	    besr.vid_buf_pitch);
  aty_st_le32(SCALER_HEIGHT_WIDTH,  besr.height_width);
  aty_st_le32(SCALER_BUF0_OFFSET,   mach64_buffer_base[0][0]);
  aty_st_le32(SCALER_BUF0_OFFSET_U, mach64_buffer_base[0][1]);
  aty_st_le32(SCALER_BUF0_OFFSET_V, mach64_buffer_base[0][2]);
  aty_st_le32(SCALER_BUF1_OFFSET,   mach64_buffer_base[0][0]);
  aty_st_le32(SCALER_BUF1_OFFSET_U, mach64_buffer_base[0][1]);
  aty_st_le32(SCALER_BUF1_OFFSET_V, mach64_buffer_base[0][2]);
  ati_wait_vsync();
  
  wait_for_fifo(4);
  aty_st_le32(OVERLAY_SCALE_CNTL, 0xC4000003);

  wait_for_idle();
  vf = aty_ld_le32(VIDEO_FORMAT);

  switch(besr.fourcc)
    {
    /* BGR formats */
    case IMGFMT_BGR15: aty_st_le32(VIDEO_FORMAT, 0x00030000); break;
    case IMGFMT_BGR16: aty_st_le32(VIDEO_FORMAT, 0x00040000); break;
    case IMGFMT_BGR32: aty_st_le32(VIDEO_FORMAT, 0x00060000); break;
    /* 4:2:0 */
    case IMGFMT_IYUV:
    case IMGFMT_I420:
    case IMGFMT_YV12:  aty_st_le32(VIDEO_FORMAT, 0x000A0000); break;
    case IMGFMT_YVU9:  aty_st_le32(VIDEO_FORMAT, 0x00090000); break;
    /* 4:2:2 */
    case IMGFMT_YVYU:
    case IMGFMT_UYVY:  aty_st_le32(VIDEO_FORMAT, 0x000C0000); break;
    case IMGFMT_YUY2:
    default:           aty_st_le32(VIDEO_FORMAT, 0x000B0000); break;
    }
}

static void
ati_stop_video(void)
{
  wait_for_fifo(14);
  aty_st_le32(OVERLAY_SCALE_CNTL, 0x80000000);
  aty_st_le32(OVERLAY_EXCLUSIVE_HORZ, 0);
  aty_st_le32(OVERLAY_EXCLUSIVE_VERT, 0);
  aty_st_le32(SCALER_H_COEFF0, 0x00002000);
  aty_st_le32(SCALER_H_COEFF1, 0x0D06200D);
  aty_st_le32(SCALER_H_COEFF2, 0x0D0A1C0D);
  aty_st_le32(SCALER_H_COEFF3, 0x0C0E1A0C);
  aty_st_le32(SCALER_H_COEFF4, 0x0C14140C);
  aty_st_le32(VIDEO_FORMAT, 0xB000B);
  aty_st_le32(SCALER_TEST, 0x0);
}

/* return "alignment of Y,U,V component" */
static unsigned
ati_query_pitch(unsigned fourcc, const vidix_yuv_t *spitch)
{
  unsigned pitch,spy,spv,spu;
  spy = spv = spu = 0;
  switch (spitch->y)
    {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spy = spitch->y; break;
    default: break;
    }
  switch(spitch->u)
    {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spu = spitch->u; break;
    default: break;
    }
  switch(spitch->v)
    {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spv = spitch->v; break;
    default: break;
    }
  switch(fourcc)
    {
    /* 4:2:0 */
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
    case IMGFMT_I420:
      if (spy > 16 && spu == spy/2 && spv == spy/2)	pitch = spy;
      else						pitch = 32/*16*/;
      break;
    case IMGFMT_YVU9:
      if (spy > 32 && spu == spy/4 && spv == spy/4)	pitch = spy;
      else						pitch = 64;
      break;
    default:
      if (spy >= 16)					pitch = spy;
      else						pitch = 16;
      break;
    }
  return pitch;
}

static int
ati_init_video(vidix_playback_t *config)
{
  l4_uint32_t src_w,src_h,dest_w,dest_h,pitch;
  l4_uint32_t h_inc,v_inc,left,leftUV,top,ecp,y_pos;
  int is_420, best_pitch,mpitch;
  int src_offset_y, src_offset_u, src_offset_v;
  unsigned int i;

  wait_for_fifo(5);
  aty_st_le32(SCALER_COLOUR_CNTL,0x00101020);

  aty_st_le32(OVERLAY_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
  aty_st_le32(OVERLAY_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
  if (besr.ckey_on)
    aty_st_le32(OVERLAY_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);
  else
    aty_st_le32(OVERLAY_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND);

  ati_stop_video();

  left  = config->src.x;
  top   = config->src.y;
  src_h = config->src.h;
  src_w = config->src.w;
  best_pitch = ati_query_pitch(config->fourcc, &config->src.pitch);
  mpitch = best_pitch-1;
  is_420 = 0;

  switch(config->fourcc)
    {
    /* 4:2:0 */
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
    case IMGFMT_I420:
      is_420 = 1;
      /* fall through */
    case IMGFMT_YVU9:
      pitch = (src_w + mpitch) & ~mpitch;
      config->dest.pitch.y = 
      config->dest.pitch.u = 
      config->dest.pitch.v = best_pitch;
      besr.vid_buf_pitch   = pitch;
      break;
    /* RGB 4:4:4:4 */
    case IMGFMT_RGB32:
    case IMGFMT_BGR32: 
      pitch = (src_w*4 + mpitch) & ~mpitch;
      config->dest.pitch.y = 
      config->dest.pitch.u = 
      config->dest.pitch.v = best_pitch;
      besr.vid_buf_pitch   = pitch>>2;
      break;
    /* 4:2:2 */
    default: 
      /* RGB15, RGB16, YVYU, UYVY, YUY2 */
      pitch = ((src_w*2) + mpitch) & ~mpitch;
      config->dest.pitch.y =
      config->dest.pitch.u =
      config->dest.pitch.v = best_pitch;
      besr.vid_buf_pitch   = pitch>>1;
      break;
    }
  dest_w = config->dest.w;
  dest_h = config->dest.h;
  besr.fourcc = config->fourcc;
  ecp = (ati_in_pll(PLL_VCLK_CNTL) & 0x30) >> 4;
  v_inc = src_h * (1<<16);

  if (ati_is_interlace())
    v_inc<<=1;
  if (ati_is_dbl_scan()) 
    v_inc>>=1;
  v_inc>>=4; // convert 16.16 -> 20.12
  v_inc /= dest_h;

  h_inc = (src_w << (12+ecp)) / dest_w;
  /* keep everything in 16.16 */
  config->offsets[0] = 0;
  for (i=1; i<config->num_frames; i++)
    config->offsets[i] = config->offsets[i-1] + config->frame_size;

  if (is_420)
    {
      config->offset.y = 0;
      config->offset.u = (pitch*src_h + 15)&~15;
      config->offset.v = (config->offset.u + (pitch*src_h>>2) + 15)&~15;

      src_offset_y = config->offset.y +  top*pitch     +  left;
      src_offset_u = config->offset.u + (top*pitch>>2) + (left>>1);
      src_offset_v = config->offset.v + (top*pitch>>2) + (left>>1);

      if (besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
	{
	  l4_uint32_t tmp;
	  tmp = config->offset.u;
	  config->offset.u = config->offset.v;
	  config->offset.v = tmp;
	}
    }
  else if (besr.fourcc == IMGFMT_YVU9)
    {
      config->offset.y = 0;
      config->offset.u = (pitch*src_h + 15)&~15; 
      config->offset.v = (config->offset.u + (pitch*src_h>>4) + 15)&~15;

      src_offset_y = config->offset.y +  top*pitch     +  left;
      src_offset_u = config->offset.u + (top*pitch>>4) + (left>>1);
      src_offset_v = config->offset.v + (top*pitch>>4) + (left>>1);
    }
  else if (besr.fourcc == IMGFMT_BGR32)
    {
      config->offset.y =
      config->offset.u =
      config->offset.v = 0;
      src_offset_y     =
      src_offset_u     =
      src_offset_v     = top*pitch + (left << 2);
    }
  else
    {
      config->offset.y = 
      config->offset.u =
      config->offset.v = 0;
      src_offset_y     = 
      src_offset_u     =
      src_offset_v     = top*pitch + (left << 1);
    }

  num_mach64_buffers = config->num_frames;
  for (i=0; i<config->num_frames; i++)
    {
      mach64_buffer_base[i][0] =
	(mach64_overlay_offset + config->offsets[i] + src_offset_y) & ~15;
      mach64_buffer_base[i][1] = 
	(mach64_overlay_offset + config->offsets[i] + src_offset_u) & ~15;
      mach64_buffer_base[i][2] = 
	(mach64_overlay_offset + config->offsets[i] + src_offset_v) & ~15;
    }

  leftUV = (left >> 17) & 15;
  left = (left >> 16) & 15;
  besr.scale_inc = ( h_inc << 16 ) | v_inc;
  y_pos = config->dest.y;
  if (ati_is_dbl_scan()) 
    y_pos*=2;
  besr.y_x_start = y_pos | (config->dest.x << 16);
  y_pos =config->dest.y + dest_h;
  if (ati_is_dbl_scan())
    y_pos*=2;
  besr.y_x_end = y_pos | ((config->dest.x + dest_w) << 16);
  besr.height_width = ((src_w - left)<<16) | (src_h - top);
  
  return 0;
}

static void
ati_compute_framesize(vidix_playback_t *info)
{
  unsigned pitch,awidth;

  pitch = ati_query_pitch(info->fourcc, &info->src.pitch);
  switch(info->fourcc)
    {
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
      awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
      info->frame_size = awidth*(info->src.h+info->src.h/2);
      break;
    case IMGFMT_YVU9:
      awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
      info->frame_size = awidth*(info->src.h+info->src.h/8);
      break;
    case IMGFMT_BGR32:
      awidth = (info->src.w*4 + (pitch-1)) & ~(pitch-1);
      info->frame_size = (awidth*info->src.h);
      break;
      /* YUY2 YVYU, RGB15, RGB16 */
    default:	
      awidth = (info->src.w*2 + (pitch-1)) & ~(pitch-1);
      info->frame_size = (awidth*info->src.h);
      break;
    }

  info->frame_size = (info->frame_size + 1024*1024 - 1) & ~(1024*1024-1);
}

static int
ati_video_playback(vidix_playback_t *info)
{
  ati_compute_framesize(info);

  if (info->num_frames>4) 
    info->num_frames=4;
  for (;info->num_frames>0; info->num_frames--)
    {
      /* align offset so we can map it as flexpage */
      mach64_overlay_offset =
	(hw_vid_mem_size - info->frame_size*info->num_frames) & 0xfff00000;
      if (mach64_overlay_offset>0)
	break;
    }
  if (info->num_frames <= 0) 
    return -L4_EINVAL;

  info->dga_addr = (char *)hw_map_vid_mem_addr + mach64_overlay_offset;
  ati_init_video(info);
  return 0;
}

static void
ati_set_grkey(const vidix_grkey_t *grkey)
{
  if(grkey->ckey.op == CKEY_TRUE)
    {
      besr.ckey_on = 1;
      switch (hw_bits)
	{
	case 15:
	  besr.graphics_key_msk=0x7FFF;
	  besr.graphics_key_clr=
	      ((grkey->ckey.blue &0xF8)>>3)
	    | ((grkey->ckey.green&0xF8)<<2)
	    | ((grkey->ckey.red  &0xF8)<<7);
	  break;
	case 16:
	  besr.graphics_key_msk=0xFFFF;
	  besr.graphics_key_clr=
	      ((grkey->ckey.blue &0xF8)>>3)
	    | ((grkey->ckey.green&0xFC)<<3)
	    | ((grkey->ckey.red  &0xF8)<<8);
	  break;
	case 24:
	  besr.graphics_key_msk=0xFFFFFF;
	  besr.graphics_key_clr=
	      ((grkey->ckey.blue &0xFF))
	    | ((grkey->ckey.green&0xFF)<<8)
	    | ((grkey->ckey.red  &0xFF)<<16);
	  break;
	case 32:
	  besr.graphics_key_msk=0xFFFFFF;
	  besr.graphics_key_clr=
	      ((grkey->ckey.blue &0xFF))
	    | ((grkey->ckey.green&0xFF)<<8)
	    | ((grkey->ckey.red  &0xFF)<<16);
	  break;
	default:
	  besr.ckey_on=0;
	  besr.graphics_key_msk=0;
	  besr.graphics_key_clr=0;
	}
    }
  else
    {
      besr.ckey_on=0;
      besr.graphics_key_msk=0;
      besr.graphics_key_clr=0;
    }

  wait_for_fifo(4);
  aty_st_le32(OVERLAY_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
  aty_st_le32(OVERLAY_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
  if (besr.ckey_on)
    aty_st_le32(OVERLAY_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);
  else
    aty_st_le32(OVERLAY_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND);
}

static void
ati_set_equalizer(const vidix_video_eq_t *eq)
{
  unsigned color_ctrl = aty_ld_le32(SCALER_COLOUR_CNTL);

  if (eq->cap & VEQ_CAP_BRIGHTNESS)
    {
      int br = eq->brightness * 64 / 1000;
      if (br < -64)
	br = -64;
      if (br > 63)
	br = 63;
      color_ctrl = (color_ctrl & 0xffffff00) | (br & 0x7f);
    }
  if (eq->cap & VEQ_CAP_SATURATION)
    {
      int sat = (eq->saturation + 1000) *16 / 1000;
      if (sat < 0)
	sat = 0;
      if (sat > 31)
	sat = 31;
      color_ctrl = (color_ctrl & 0xff0000ff) | (sat << 8) | (sat << 16);
    }

  aty_st_le32(SCALER_COLOUR_CNTL, color_ctrl);
}

void
ati_vid_init(con_accel_t *accel)
{
  supports_planar = 0;
  wait_for_idle();
  wait_for_fifo(2);
  if (aty_ld_le32(SCALER_BUF0_OFFSET_U))
    supports_planar = 1;
  else
    {
      aty_st_le32(SCALER_BUF0_OFFSET_U, -1);
      ati_wait_vsync();
      wait_for_idle();
      wait_for_fifo(2);
      if (aty_ld_le32(SCALER_BUF0_OFFSET_U))
	supports_planar = 1;
    }

  if (supports_planar)
    {
      accel->cscs_init  = ati_video_playback;
      accel->cscs_start = ati_start_video;
      accel->cscs_stop  = ati_stop_video;
      accel->cscs_grkey = ati_set_grkey;
      accel->caps      |= ACCEL_FAST_CSCS_YV12 | ACCEL_COLOR_KEY;
      accel->cscs_eq    = ati_set_equalizer;
      accel->caps      |= ACCEL_EQ_BRIGHTNESS | ACCEL_EQ_SATURATION;
    }
}

