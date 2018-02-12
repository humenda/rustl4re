/*!
 * \file	ati128_vid.c
 * \brief	Hardware Acceleration for ATI Rage128 cards (backend scaler)
 *
 * \date	08/2003
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
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
#include "ati128.h"
#include "aty128.h"
#include "fourcc.h"
#include "vidix.h"

typedef struct
{
  /* base address of yuv framebuffer */
  l4_uint32_t yuv_base;
  l4_uint32_t fourcc;
  /* YUV BES registers */
  l4_uint32_t reg_load_cntl;
  l4_uint32_t h_inc;
  l4_uint32_t step_by;
  l4_uint32_t y_x_start;
  l4_uint32_t y_x_end;
  l4_uint32_t v_inc;
  l4_uint32_t p1_blank_lines_at_top;
  l4_uint32_t p23_blank_lines_at_top;
  l4_uint32_t vid_buf_pitch0_value;
  l4_uint32_t vid_buf_pitch1_value;
  l4_uint32_t p1_x_start_end;
  l4_uint32_t p2_x_start_end;
  l4_uint32_t p3_x_start_end;
  l4_uint32_t base_addr;
  l4_uint32_t vid_buf_base_adrs_y[10];
  l4_uint32_t vid_buf_base_adrs_u[10];
  l4_uint32_t vid_buf_base_adrs_v[10];
  l4_uint32_t vid_nbufs;
  l4_uint32_t p1_v_accum_init;
  l4_uint32_t p1_h_accum_init;
  l4_uint32_t p23_v_accum_init;
  l4_uint32_t p23_h_accum_init;
  l4_uint32_t scale_cntl;
  l4_uint32_t exclusive_horz;
  l4_uint32_t auto_flip_cntl;
  l4_uint32_t key_cntl;
  l4_uint32_t test;
  
  /* Configurable stuff */
  int brightness;
  int saturation;
  
  int ckey_on;
  l4_uint32_t graphics_key_clr;
  l4_uint32_t graphics_key_msk;
  l4_uint32_t ckey_cntl;
  
} bes_registers_t;

static bes_registers_t besr;
static l4_int32_t ati128_overlay_off;


static void
ati128_start_video(void)
{
  l4_uint32_t bes_flags;

  aty128_wait_for_fifo(1);
  aty_st_le32(OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK);

  aty128_wait_for_idle();
  while (!(aty_ld_le32(OV0_REG_LOAD_CNTL) & REG_LD_CTL_LOCK_READBACK))
    ;

  aty128_wait_for_fifo(10);
  aty_st_le32(FCP_CNTL,                 FCP_CNTL__GND);
  aty_st_le32(CAP0_TRIG_CNTL,           0);
  aty_st_le32(VID_BUFFER_CONTROL,       (1<<16) | 0x01);
  aty_st_le32(DISP_TEST_DEBUG_CNTL,     0);
  aty_st_le32(OV0_AUTO_FLIP_CNTL,       OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD);
  aty_st_le32(OV0_COLOUR_CNTL,          0x00101000);

  aty128_wait_for_fifo(12);
  aty_st_le32(OV0_H_INC,                besr.h_inc);
  aty_st_le32(OV0_STEP_BY,              besr.step_by);
  aty_st_le32(OV0_Y_X_START,            besr.y_x_start);
  aty_st_le32(OV0_Y_X_END,              besr.y_x_end);
  aty_st_le32(OV0_V_INC,                besr.v_inc);
  aty_st_le32(OV0_P1_BLANK_LINES_AT_TOP, besr.p1_blank_lines_at_top);
  aty_st_le32(OV0_P23_BLANK_LINES_AT_TOP, besr.p23_blank_lines_at_top);
  aty_st_le32(OV0_VID_BUF_PITCH0_VALUE, besr.vid_buf_pitch0_value);
  aty_st_le32(OV0_VID_BUF_PITCH1_VALUE, besr.vid_buf_pitch1_value);
  aty_st_le32(OV0_P1_X_START_END,       besr.p1_x_start_end);
  aty_st_le32(OV0_P2_X_START_END,       besr.p2_x_start_end);
  aty_st_le32(OV0_P3_X_START_END,       besr.p3_x_start_end);

  aty128_wait_for_fifo(10);
  aty_st_le32(OV0_VID_BUF0_BASE_ADRS,   besr.vid_buf_base_adrs_y[0]);
  aty_st_le32(OV0_VID_BUF1_BASE_ADRS,   besr.vid_buf_base_adrs_u[0]);
  aty_st_le32(OV0_VID_BUF2_BASE_ADRS,   besr.vid_buf_base_adrs_v[0]);
  aty_st_le32(OV0_VID_BUF3_BASE_ADRS,   besr.vid_buf_base_adrs_y[0]);
  aty_st_le32(OV0_VID_BUF4_BASE_ADRS,   besr.vid_buf_base_adrs_u[0]);
  aty_st_le32(OV0_VID_BUF5_BASE_ADRS,   besr.vid_buf_base_adrs_v[0]);
  aty_st_le32(OV0_P1_V_ACCUM_INIT,      besr.p1_v_accum_init);
  aty_st_le32(OV0_P1_H_ACCUM_INIT,      besr.p1_h_accum_init);
  aty_st_le32(OV0_P23_V_ACCUM_INIT,     besr.p23_v_accum_init);
  aty_st_le32(OV0_P23_H_ACCUM_INIT,     besr.p23_h_accum_init);

  bes_flags = SCALER_ENABLE
	    | SCALER_SMART_SWITCH
	    | SCALER_Y2R_TEMP
	    | SCALER_PIX_EXPAND
	    | SCALER_BURST_PER_PLANE;

  switch(besr.fourcc)
    {
    case IMGFMT_RGB15:
    case IMGFMT_BGR15: bes_flags |= SCALER_SOURCE_15BPP; break;
    case IMGFMT_RGB16:
    case IMGFMT_BGR16: bes_flags |= SCALER_SOURCE_16BPP; break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32: bes_flags |= SCALER_SOURCE_32BPP; break;
    /* 4:1:0 */
    case IMGFMT_IF09:
    case IMGFMT_YVU9:  bes_flags |= SCALER_SOURCE_YUV9; break;
    /* 4:0:0 */
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    /* 4:2:0 */
    case IMGFMT_IYUV:
    case IMGFMT_I420:
    case IMGFMT_YV12:  bes_flags |= SCALER_SOURCE_YUV12; break;
    /* 4:2:2 */
    case IMGFMT_YVYU:
    case IMGFMT_UYVY:  bes_flags |= SCALER_SOURCE_YVYU422; break;
    case IMGFMT_YUY2:
    default:           bes_flags |= SCALER_SOURCE_VYUY422; break;
    }

  aty128_wait_for_fifo(2);
  aty_st_le32(OV0_SCALE_CNTL, bes_flags);
  aty_st_le32(OV0_REG_LOAD_CNTL, 0);
}

static void
ati128_stop_video(void)
{
  aty128_wait_for_idle();
  aty_st_le32(OV0_SCALE_CNTL, SCALER_SOFT_RESET);
  aty_st_le32(OV0_EXCLUSIVE_HORZ, 0);
  aty_st_le32(OV0_AUTO_FLIP_CNTL, 0);
  aty_st_le32(OV0_FILTER_CNTL, FILTER_HARDCODED_COEF);
  aty_st_le32(OV0_KEY_CNTL, GRAPHIC_KEY_FN_NE);
  aty_st_le32(OV0_TEST, 0);
}

/* return "alignment of Y,U,V component" */
static unsigned
ati128_query_pitch(unsigned fourcc, const vidix_yuv_t *spitch)
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
ati128_init_video(vidix_playback_t *config)
{
  l4_uint32_t i,tmp,src_w,src_h,dest_w,dest_h,pitch;
  l4_uint32_t h_inc,step_by,left,leftUV,top;
  int is_400,is_410,is_420,is_rgb32,is_rgb,best_pitch,mpitch;
  unsigned int ecp_div;

  aty128_wait_for_fifo(3);

  aty_st_le32(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
  aty_st_le32(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
  if (besr.ckey_on)
    aty_st_le32(OV0_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);
  else
    aty_st_le32(OV0_KEY_CNTL,
		VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND);

  ati128_stop_video();
  
  left   = config->src.x << 16;
  top    = config->src.y << 16;
  src_h  = config->src.h;
  src_w  = config->src.w;
  is_400 = is_410 = is_420 = is_rgb32 = is_rgb = 0;
  if (config->fourcc == IMGFMT_YV12 ||
      config->fourcc == IMGFMT_I420 ||
      config->fourcc == IMGFMT_IYUV) 
    is_420 = 1;
  if (config->fourcc == IMGFMT_YVU9 ||
      config->fourcc == IMGFMT_IF09)
    is_410 = 1;
  if (config->fourcc == IMGFMT_Y800 ||
      config->fourcc == IMGFMT_Y8) 
    is_400 = 1;
  if (config->fourcc == IMGFMT_RGB32 ||
      config->fourcc == IMGFMT_BGR32)
    is_rgb32 = 1;
  if (config->fourcc == IMGFMT_RGB32 ||
      config->fourcc == IMGFMT_BGR32 ||
      config->fourcc == IMGFMT_RGB24 ||
      config->fourcc == IMGFMT_BGR24 ||
      config->fourcc == IMGFMT_RGB16 ||
      config->fourcc == IMGFMT_BGR16 ||
      config->fourcc == IMGFMT_RGB15 ||
      config->fourcc == IMGFMT_BGR15)
    is_rgb = 1;

  best_pitch = ati128_query_pitch(config->fourcc,&config->src.pitch);
  mpitch = best_pitch-1;
  switch(config->fourcc)
    {
      /* 4:0:0 */
    case IMGFMT_Y800:
    case IMGFMT_Y8:
      /* 4:1:0 */
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
      /* 4:2:0 */
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
    case IMGFMT_I420: pitch = (src_w + mpitch) & ~mpitch;
		      config->dest.pitch.y = 
		      config->dest.pitch.u = 
		      config->dest.pitch.v = best_pitch;
		      break;
    /* RGB 4:4:4:4 */
    case IMGFMT_RGB32:
    case IMGFMT_BGR32: pitch = (src_w*4 + mpitch) & ~mpitch;
		       config->dest.pitch.y = 
		       config->dest.pitch.u = 
		       config->dest.pitch.v = best_pitch;
		       break;
    /* 4:2:2 */
    default: /* RGB15, RGB16, YVYU, UYVY, YUY2 */
		       pitch = ((src_w*2) + mpitch) & ~mpitch;
		       config->dest.pitch.y =
		       config->dest.pitch.u =
		       config->dest.pitch.v = best_pitch;
		       break;
    }
  dest_w = config->dest.w;
  dest_h = config->dest.h;
  besr.fourcc = config->fourcc;
  besr.v_inc = (src_h << 20) / dest_h;
  h_inc = (src_w << 12) / dest_w;

  ecp_div = (aty_in_pll(VCLK_ECP_CNTL) >> 8) & 3;
  h_inc <<= ecp_div;

  step_by = 1;
  while (h_inc >= (2 << 12)) 
    {
      step_by++;
      h_inc >>= 1;
    }

  /* keep everything in 16.16 */
  besr.base_addr = aty_ld_le32(DISPLAY_BASE_ADDR);
  config->offsets[0] = 0;
  for (i=1; i<besr.vid_nbufs; i++)
    config->offsets[i] = config->offsets[i-1]+config->frame_size;

  if (is_420 || is_410 || is_400)
    {
      l4_uint32_t d1line,d2line,d3line;

      d1line = top*pitch;
      if (is_420)
	{
	  d2line = src_h*pitch+(d1line>>2);
	  d3line = d2line+((src_h*pitch)>>2);
	}
      else if (is_410)
	{
       	  d2line = src_h*pitch+(d1line>>4);
	  d3line = d2line+((src_h*pitch)>>4);
	}
      else
	{
	  d2line = 0;
	  d3line = 0;
	}
      d1line += (left >> 16) & ~15;
      if (is_420)
	{
	  d2line += (left >> 17) & ~15;
	  d3line += (left >> 17) & ~15;
	}
      else if(is_410)
	{
       	  d2line += (left >> 18) & ~15;
	  d3line += (left >> 18) & ~15;
	}

      config->offset.y = d1line & VIF_BUF0_BASE_ADRS_MASK;
      if (is_400)
	{
	  config->offset.v = 0;
	  config->offset.u = 0;
	}
      else
	{
	  config->offset.v = d2line & VIF_BUF1_BASE_ADRS_MASK;
	  config->offset.u = d3line & VIF_BUF2_BASE_ADRS_MASK;
	}

      for (i=0; i<besr.vid_nbufs; i++)
	{
	  besr.vid_buf_base_adrs_y[i] = 
	    ((ati128_overlay_off + config->offsets[i]
	      + config->offset.y) & VIF_BUF0_BASE_ADRS_MASK);
	  if (is_400)
	    {
	      besr.vid_buf_base_adrs_v[i]=0;
	      besr.vid_buf_base_adrs_u[i]=0;
	    }
	  else
	    {
	      if (besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
		{
		  besr.vid_buf_base_adrs_u[i] = 
		    ((ati128_overlay_off + config->offsets[i] 
		      + config->offset.v) & VIF_BUF1_BASE_ADRS_MASK)
		    | VIF_BUF1_PITCH_SEL;
		  besr.vid_buf_base_adrs_v[i] = 
		    ((ati128_overlay_off + config->offsets[i]
		      + config->offset.u) & VIF_BUF2_BASE_ADRS_MASK)
		    | VIF_BUF2_PITCH_SEL;
		}
	      else
		{
		  besr.vid_buf_base_adrs_v[i] = 
		    ((ati128_overlay_off + config->offsets[i]
		      + config->offset.v) & VIF_BUF1_BASE_ADRS_MASK)
		    | VIF_BUF1_PITCH_SEL;
		  besr.vid_buf_base_adrs_u[i] =
		    ((ati128_overlay_off + config->offsets[i]
		      + config->offset.u) & VIF_BUF2_BASE_ADRS_MASK)
		    | VIF_BUF2_PITCH_SEL;
		}
	    }
	}
      config->offset.y = (besr.vid_buf_base_adrs_y[0] 
			  & VIF_BUF0_BASE_ADRS_MASK) - ati128_overlay_off;
      if (is_400)
	{
	  config->offset.v = 0;
	  config->offset.u = 0;
	}
      else
	{
	  config->offset.v = (besr.vid_buf_base_adrs_v[0]
			      & VIF_BUF1_BASE_ADRS_MASK) - ati128_overlay_off;
	  config->offset.u = (besr.vid_buf_base_adrs_u[0]
			      & VIF_BUF2_BASE_ADRS_MASK) - ati128_overlay_off;
	}
    }
  else
    {
      config->offset.y =
      config->offset.u =
      config->offset.v = ((left & ~7) << 1) & VIF_BUF0_BASE_ADRS_MASK;
      for (i=0; i<besr.vid_nbufs; i++)
	{
	  besr.vid_buf_base_adrs_y[i] =
	  besr.vid_buf_base_adrs_u[i] =
	  besr.vid_buf_base_adrs_v[i] = ati128_overlay_off 
				      + config->offsets[i] + config->offset.y;
	}
    }

  tmp = (left & 0x0003ffff) + 0x00028000 + (h_inc << 3);
  besr.p1_h_accum_init = ((tmp <<  4) & 0x000f8000) 
		       | ((tmp << 12) & 0xf0000000);

  tmp = ((left >> 1) & 0x0001ffff) + 0x00028000 + (h_inc << 2);
  besr.p23_h_accum_init = ((tmp <<  4) & 0x000f8000) 
			| ((tmp << 12) & 0x70000000);
  tmp = (top & 0x0000ffff) + 0x00018000;
  besr.p1_v_accum_init = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK)
		       | (OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);

  tmp = ((top >> 1) & 0x0000ffff) + 0x00018000;
  besr.p23_v_accum_init =  (is_420||is_410) 
			      ? ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK) 
			        |(OV0_P23_MAX_LN_IN_PER_LN_OUT & 1) 
			      : 0;

  leftUV = (left >> (is_410?18:17)) & 15;
  left   = (left >> 16) & 15;
  if (is_rgb && !is_rgb32)
    h_inc<<=1;
  if (is_rgb32)
    besr.h_inc = (h_inc >> 1) | ((h_inc >> 1) << 16);
  else if (is_410)
    besr.h_inc = h_inc | ((h_inc >> 2) << 16);
  else
    besr.h_inc = h_inc | ((h_inc >> 1) << 16);
  besr.step_by = step_by | (step_by << 8);
  besr.y_x_start = config->dest.x 
		 | (config->dest.y << 16);
  besr.y_x_end   = (config->dest.x + dest_w) 
		 | ((config->dest.y + dest_h) << 16);
  besr.p1_blank_lines_at_top = P1_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
  if (is_420 || is_410)
    {
      src_h = (src_h + 1) >> (is_410?2:1);
      besr.p23_blank_lines_at_top = P23_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
    }
  else
    besr.p23_blank_lines_at_top = 0;

  besr.vid_buf_pitch0_value = pitch;
  besr.vid_buf_pitch1_value = is_410 ? pitch>>2 : is_420 ? pitch>>1 : pitch;
  besr.p1_x_start_end = (src_w+left-1)|(left<<16);
  if (is_410||is_420)
    src_w>>=is_410 ? 2 : 1;
  if (is_400)
    {
      besr.p2_x_start_end = 0;
      besr.p3_x_start_end = 0;
    }
  else
    {
      besr.p2_x_start_end = (src_w+left-1)|(leftUV<<16);
      besr.p3_x_start_end = besr.p2_x_start_end;
    }

  return 0;
}

static void
ati128_compute_framesize(vidix_playback_t *info)
{
  unsigned pitch,awidth;

  pitch = ati128_query_pitch(info->fourcc, &info->src.pitch);
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
ati128_video_playback(vidix_playback_t *info)
{
  ati128_compute_framesize(info);

  if (info->num_frames>4)
    info->num_frames=4;

  for (; info->num_frames>0; info->num_frames--)
    {
      /* align offset so we can map it as flexpage */
      ati128_overlay_off =
	(hw_vid_mem_size - info->frame_size*info->num_frames) & 0xfff00000;
      if (ati128_overlay_off>0)
	break;
    }
  if (info->num_frames <= 0) 
    return -L4_EINVAL;

  besr.vid_nbufs = info->num_frames;
  info->dga_addr = (char *)hw_map_vid_mem_addr + ati128_overlay_off;
  ati128_init_video(info);
  return 0;
}

  static void
ati128_set_grkey(const vidix_grkey_t *grkey)
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

  aty128_wait_for_fifo(3);
  aty_st_le32(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
  aty_st_le32(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
  if (besr.ckey_on)
    aty_st_le32(OV0_KEY_CNTL,
	VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);
  else
    aty_st_le32(OV0_KEY_CNTL,
	VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND);
}

  static void
ati128_set_equalizer(const vidix_video_eq_t *eq)
{
  unsigned color_ctrl = aty_ld_le32(OV0_COLOUR_CNTL);

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

  aty_st_le32(OV0_COLOUR_CNTL, color_ctrl);
}

  void
ati128_vid_init(con_accel_t *accel)
{
  accel->cscs_init  = ati128_video_playback;
  accel->cscs_start = ati128_start_video;
  accel->cscs_stop  = ati128_stop_video;
  accel->cscs_grkey = ati128_set_grkey;
  accel->caps      |= ACCEL_FAST_CSCS_YV12 | ACCEL_COLOR_KEY;
  accel->cscs_eq    = ati128_set_equalizer;
  accel->caps      |= ACCEL_EQ_BRIGHTNESS | ACCEL_EQ_SATURATION;
}

