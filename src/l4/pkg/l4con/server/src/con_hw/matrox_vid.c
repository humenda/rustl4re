/*!
 * \file	matrox_vid.c
 * \brief	Hardware Acceleration for Matrox Gxx cards (backend scaler)
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

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <string.h>

#include "init.h"
#include "matrox.h"
#include "matrox_regs.h"
#include "vidix.h"
#include "fourcc.h"

static l4_addr_t mga_src_base = 0;
static int colkey_saved = 0;
static int colkey_on = 0;
static unsigned char colkey_color[4];
static unsigned char colkey_mask[4];
static vidix_grkey_t mga_grkey;

extern int is_g400;

#define XMULCTRL      0x19
#define BPP_8         0x00
#define BPP_15        0x01
#define BPP_16        0x02
#define BPP_24        0x03
#define BPP_32_DIR    0x04
#define BPP_32_PAL    0x07

#define XCOLMSK       0x40
#define X_COLKEY      0x42
#define XKEYOPMODE    0x51
#define XCOLMSK0RED   0x52
#define XCOLMSK0GREEN 0x53
#define XCOLMSK0BLUE  0x54
#define XCOLKEY0RED   0x55
#define XCOLKEY0GREEN 0x56
#define XCOLKEY0BLUE  0x57

#define VCOUNT      0x1e20

#define PALWTADD    0x3c00 // Index register for X_DATAREG port
#define X_DATAREG   0x3c0a

// Backend Scaler registers
#define BESCTL      0x3d20
#define BESGLOBCTL  0x3dc0
#define BESLUMACTL  0x3d40
#define BESPITCH    0x3d24

#define BESA1C3ORG  0x3d60
#define BESA1CORG   0x3d10
#define BESA1ORG    0x3d00

#define BESA2C3ORG  0x3d64 
#define BESA2CORG   0x3d14
#define BESA2ORG    0x3d04

#define BESB1C3ORG  0x3d68
#define BESB1CORG   0x3d18
#define BESB1ORG    0x3d08

#define BESB2C3ORG  0x3d6C
#define BESB2CORG   0x3d1C
#define BESB2ORG    0x3d0C

#define BESHCOORD   0x3d28
#define BESHISCAL   0x3d30
#define BESHSRCEND  0x3d3C
#define BESHSRCLST  0x3d50
#define BESHSRCST   0x3d38
#define BESV1WGHT   0x3d48
#define BESV2WGHT   0x3d4c
#define BESV1SRCLST 0x3d54
#define BESV2SRCLST 0x3d58
#define BESVISCAL   0x3d34
#define BESVCOORD   0x3d2c
#define BESSTATUS   0x3dc4


/* MATROX BES registers */
typedef struct
{
  l4_uint32_t besctl;	   //BES Control
  l4_uint32_t besglobctl;  //BES Global control
  l4_uint32_t beslumactl;  //Luma control (brightness and contrast)
  l4_uint32_t bespitch;	   //Line pitch
  
  l4_uint32_t besa1c3org;  //Buffer A-1 Chroma 3 plane org
  l4_uint32_t besa1corg;   //Buffer A-1 Chroma org
  l4_uint32_t besa1org;	   //Buffer A-1 Luma org
  
  l4_uint32_t besa2c3org;  //Buffer A-2 Chroma 3 plane org
  l4_uint32_t besa2corg;   //Buffer A-2 Chroma org
  l4_uint32_t besa2org;	   //Buffer A-2 Luma org
  
  l4_uint32_t besb1c3org;  //Buffer B-1 Chroma 3 plane org
  l4_uint32_t besb1corg;   //Buffer B-1 Chroma org
  l4_uint32_t besb1org;	   //Buffer B-1 Luma org
  
  l4_uint32_t besb2c3org;  //Buffer B-2 Chroma 3 plane org
  l4_uint32_t besb2corg;   //Buffer B-2 Chroma org
  l4_uint32_t besb2org;	   //Buffer B-2 Luma org
  
  l4_uint32_t beshcoord;   //BES Horizontal coord
  l4_uint32_t beshiscal;   //BES Horizontal inverse scaling [5.14]
  l4_uint32_t beshsrcst;   //BES Horizontal source start [10.14] (for scaling)
  l4_uint32_t beshsrcend;  //BES Horizontal source ending [10.14] (for scaling) 
  l4_uint32_t beshsrclst;  //BES Horizontal source last 
  
  l4_uint32_t besvcoord;   //BES Vertical coord
  l4_uint32_t besviscal;   //BES Vertical inverse scaling [5.14]
  l4_uint32_t besv1srclst; //BES Field 1 vertical source last position
  l4_uint32_t besv1wght;   //BES Field 1 weight start
  l4_uint32_t besv2srclst; //BES Field 2 vertical source last position
  l4_uint32_t besv2wght;   //BES Field 2 weight start
} bes_registers_t;

static bes_registers_t regs;


static void
mga_vid_write_regs(int restore)
{
  //Make sure internal registers don't get updated until we're done
  mga_outl(BESGLOBCTL, (mga_inl(VCOUNT)-1)<<16);

  // color or coordinate keying
  if(restore && colkey_saved)
    {
      // restore it
      colkey_saved=0;

      // Set color key registers:
      mga_outb(PALWTADD, XKEYOPMODE);
      mga_outb(X_DATAREG, colkey_on);

      mga_outb(PALWTADD, XCOLKEY0RED);
      mga_outb(X_DATAREG, colkey_color[0]);
      mga_outb(PALWTADD, XCOLKEY0GREEN);
      mga_outb(X_DATAREG, colkey_color[1]);
      mga_outb(PALWTADD, XCOLKEY0BLUE);
      mga_outb(X_DATAREG, colkey_color[2]);
      mga_outb(PALWTADD, X_COLKEY);
      mga_outb(X_DATAREG, colkey_color[3]);
      
      mga_outb(PALWTADD, XCOLMSK0RED);
      mga_outb(X_DATAREG, colkey_mask[0]);
      mga_outb(PALWTADD, XCOLMSK0GREEN);
      mga_outb(X_DATAREG, colkey_mask[1]);
      mga_outb(PALWTADD, XCOLMSK0BLUE);
      mga_outb(X_DATAREG, colkey_mask[2]);
      mga_outb(PALWTADD, XCOLMSK);
      mga_outb(X_DATAREG, colkey_mask[3]);
    } 
  else if(!colkey_saved)
    {
      // save it
      colkey_saved=1;
      // Get color key registers:
      mga_outb(PALWTADD, XKEYOPMODE);
      colkey_on = mga_inb(X_DATAREG) & 1;
      
      mga_outb(PALWTADD, XCOLKEY0RED);
      colkey_color[0] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, XCOLKEY0GREEN);
      colkey_color[1] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, XCOLKEY0BLUE);
      colkey_color[2] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, X_COLKEY);
      colkey_color[3] = mga_inb(X_DATAREG);
      
      mga_outb(PALWTADD, XCOLMSK0RED);
      colkey_mask[0] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, XCOLMSK0GREEN);
      colkey_mask[1] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, XCOLMSK0BLUE);
      colkey_mask[2] = mga_inb(X_DATAREG);
      mga_outb(PALWTADD, XCOLMSK);
      colkey_mask[3] = mga_inb(X_DATAREG);
    }

  if(!restore)
    {
      mga_outb(PALWTADD, XKEYOPMODE);
      mga_outb(X_DATAREG, mga_grkey.ckey.op == CKEY_TRUE);
      if (mga_grkey.ckey.op == CKEY_TRUE)
	{
  	  l4_uint32_t r=0, g=0, b=0;

	  mga_outb(PALWTADD, XMULCTRL);
	  switch (mga_inb(X_DATAREG)) 
	    {
	    case BPP_8:
	      /* Need to look up the color index, just using
		 color 0 for now. */
	      break;
	      
	    case BPP_15:
	      r = mga_grkey.ckey.red   >> 3;
	      g = mga_grkey.ckey.green >> 3;
      	      b = mga_grkey.ckey.blue  >> 3;
	      break;

	    case BPP_16:
      	      r = mga_grkey.ckey.red   >> 3;
	      g = mga_grkey.ckey.green >> 2;
	      b = mga_grkey.ckey.blue  >> 3;
	      break;

    	    case BPP_24:
	    case BPP_32_DIR:
	    case BPP_32_PAL:
	      r = mga_grkey.ckey.red;
	      g = mga_grkey.ckey.green;
	      b = mga_grkey.ckey.blue;
	      break;
	    }

  	  // Disable color keying on alpha channel 
	  mga_outb(PALWTADD, XCOLMSK);
	  mga_outb(X_DATAREG, 0x00);
	  mga_outb(PALWTADD, X_COLKEY);
	  mga_outb(X_DATAREG, 0x00);

  	  // Set up color key registers
	  mga_outb(PALWTADD, XCOLKEY0RED);
	  mga_outb(X_DATAREG, r);
	  mga_outb(PALWTADD, XCOLKEY0GREEN);
	  mga_outb(X_DATAREG, g);
	  mga_outb(PALWTADD, XCOLKEY0BLUE);
	  mga_outb(X_DATAREG, b);

	  // Set up color key mask registers
	  mga_outb(PALWTADD, XCOLMSK0RED);
	  mga_outb(X_DATAREG, 0xff);
	  mga_outb(PALWTADD, XCOLMSK0GREEN);
	  mga_outb(X_DATAREG, 0xff);
	  mga_outb(PALWTADD, XCOLMSK0BLUE);
	  mga_outb(X_DATAREG, 0xff);
	}
    }

  // Backend Scaler
  mga_outl(BESCTL,    regs.besctl);
  if(is_g400)
    mga_outl(BESLUMACTL, regs.beslumactl);
  mga_outl(BESPITCH,  regs.bespitch);
  mga_outl(BESA1ORG,  regs.besa1org);
  mga_outl(BESA1CORG, regs.besa1corg);
  mga_outl(BESA2ORG,  regs.besa2org);
  mga_outl(BESA2CORG, regs.besa2corg);
  mga_outl(BESB1ORG,  regs.besb1org);
  mga_outl(BESB1CORG, regs.besb1corg);
  mga_outl(BESB2ORG,  regs.besb2org);
  mga_outl(BESB2CORG, regs.besb2corg);
  if(is_g400)
    {
      mga_outl(BESA1C3ORG, regs.besa1c3org);
      mga_outl(BESA2C3ORG, regs.besa2c3org);
      mga_outl(BESB1C3ORG, regs.besb1c3org);
      mga_outl(BESB2C3ORG, regs.besb2c3org);
    }
  mga_outl(BESHCOORD,   regs.beshcoord);
  mga_outl(BESHISCAL,   regs.beshiscal);
  mga_outl(BESHSRCST,   regs.beshsrcst);
  mga_outl(BESHSRCEND,  regs.beshsrcend);
  mga_outl(BESHSRCLST,  regs.beshsrclst);
  mga_outl(BESVCOORD,   regs.besvcoord);
  mga_outl(BESVISCAL,   regs.besviscal);
  mga_outl(BESV1SRCLST, regs.besv1srclst);
  mga_outl(BESV1WGHT,   regs.besv1wght);
  mga_outl(BESV2SRCLST, regs.besv2srclst);
  mga_outl(BESV2WGHT,   regs.besv2wght);
  
  //update the registers somewhere between 1 and 2 frames from now.
  mga_outl(BESGLOBCTL,  regs.besglobctl + ((mga_inb(VCOUNT)+2)<<16));
}

static void
matrox_start_video(void)
{
}

static void
matrox_stop_video(void)
{
  regs.besctl &= ~1;
  regs.besglobctl &= ~(1<<6);
  mga_vid_write_regs(0);
}

static int
matrox_video_playback(vidix_playback_t *config)
{
  unsigned i;
  int x, y, sw, sh, dw, dh;
  int besleft, bestop, ifactor, ofsleft, ofstop, baseadrofs, weight, weights;

  if ((config->num_frames < 1) || (config->num_frames > 4))
    config->num_frames = 1;

  x  = config->dest.x;
  y  = config->dest.y;
  sw = config->src.w;
  sh = config->src.h;
  dw = config->dest.w;
  dh = config->dest.h;

  config->dest.pitch.y = /*32*/16;
  config->dest.pitch.u = 
  config->dest.pitch.v = /*16*/4;

  if ((sw < 4) || (sh < 4) || (dw < 4) || (dh < 4))
    return -L4_EINVAL;

  sw += sw & 1;
  switch(config->fourcc)
    {
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
      sh += sh & 1;
      config->frame_size =  ((sw + 31) & ~31) * sh
			 + (((sw + 31) & ~31) * sh) / 2;
      break;
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
      config->frame_size = ((sw + 31) & ~31) * sh * 2;
      break;
    default:
      return -L4_EINVAL;
    }

  /* make it possible to map offscreen area using L4 flexpage */
  config->frame_size = (config->frame_size + 1024*1024 - 1) & ~(1024*1024-1);

  config->offsets[0] = 0;
  for (i = 1; i < config->num_frames+1; i++)
    config->offsets[i] = i*config->frame_size;

  config->offset.y = 0;
//  config->offset.u = ((sw + 15) & ~15) * sh;
  config->offset.u = ((sw + 31) & ~31) * sh;
//  config->offset.v = config->offset.u+((sw + 15) & ~15) * sh /4;
  config->offset.v = config->offset.u+((sw + 31) & ~31) * sh /4;

  mga_src_base = hw_vid_mem_size - config->num_frames*config->frame_size;
  mga_src_base &= ~(1024*1024-1); /* 1MB boundary */

  config->dga_addr = (char*)hw_map_vid_mem_addr + mga_src_base;

  /* for G200 set Interleaved UV planes */
  if (!is_g400)
    config->flags = VID_PLAY_INTERLEAVED_UV | INTERLEAVING_UV;

  regs.besglobctl = 0;

  switch(config->fourcc)
    {
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
      regs.besctl = 1         // BES enabled
		  + (0<<6)    // even start polarity
		  + (1<<10)   // x filtering enabled
		  + (1<<11)   // y filtering enabled
		  + (1<<16)   // chroma upsampling
		  + (1<<17)   // 4:2:0 mode
		  + (1<<18);  // dither enabled
      break;

    case IMGFMT_YUY2:
      regs.besctl = 1         // BES enabled
                  + (0<<6)    // even start polarity
	       	  + (1<<10)   // x filtering enabled
	     	  + (1<<11)   // y filtering enabled
	   	  + (1<<16)   // chroma upsampling
	 	  + (0<<17)   // 4:2:2 mode
       		  + (1<<18);  // dither enabled
      regs.besglobctl = 0;    // YUY2 format selected
      break;

    case IMGFMT_UYVY:
      regs.besctl = 1         // BES enabled
                  + (0<<6)    // even start polarity
	       	  + (1<<10)   // x filtering enabled
	     	  + (1<<11)   // y filtering enabled
	   	  + (1<<16)   // chroma upsampling
	 	  + (0<<17)   // 4:2:2 mode
       		  + (1<<18);  // dither enabled
      regs.besglobctl = 1<<6; // UYVY format selected
      break;
    }

  //Disable contrast and brightness control
  regs.besglobctl |= (1<<5) + (1<<7);
  regs.beslumactl = (0x7f << 16) + (0x80<<0);
  regs.beslumactl = 0x80<<0;

  //Setup destination window boundaries
  besleft = x > 0 ? x : 0;
  bestop  = y > 0 ? y : 0;
  regs.beshcoord = (besleft<<16) + (x + dw-1);
  regs.besvcoord = (bestop <<16) + (y + dh-1);

  //Setup source dimensions
  regs.beshsrclst = (sw - 1) << 16;
  regs.bespitch   = (sw + 31) & ~31;
//  regs.bespitch   = (sw + 15) & ~15; 

  //Setup horizontal scaling
  ifactor = ((sw-1)<<14)/(dw-1);
  ofsleft = besleft - x;

  regs.beshiscal = ifactor<<2;
  regs.beshsrcst = (ofsleft*ifactor)<<2;
  regs.beshsrcend = regs.beshsrcst + (((dw - ofsleft - 1) * ifactor) << 2);

  //Setup vertical scaling
  ifactor = ((sh-1)<<14)/(dh-1);
  ofstop = bestop - y;

  regs.besviscal = ifactor<<2;

  baseadrofs = ((ofstop*regs.besviscal)>>16)*regs.bespitch;
  regs.besa1org = mga_src_base + baseadrofs;
  regs.besa2org = mga_src_base + baseadrofs + 1*config->frame_size;
  regs.besb1org = mga_src_base + baseadrofs + 2*config->frame_size;
  regs.besb2org = mga_src_base + baseadrofs + 3*config->frame_size;

  if(   config->fourcc==IMGFMT_YV12
      ||config->fourcc==IMGFMT_IYUV
      ||config->fourcc==IMGFMT_I420)
    {
      // planar YUV frames:
      if (is_g400) 
	baseadrofs = (((ofstop*regs.besviscal)/4)>>16)*regs.bespitch;
      else 
	baseadrofs = (((ofstop*regs.besviscal)/2)>>16)*regs.bespitch;

      if (config->fourcc == IMGFMT_YV12)
	{
  	  regs.besa1corg = mga_src_base + baseadrofs
			 + 			  regs.bespitch * sh;
	  regs.besa2corg = mga_src_base + baseadrofs
	    		 + 1*config->frame_size + regs.bespitch * sh;
	  regs.besb1corg = mga_src_base + baseadrofs 
			 + 2*config->frame_size + regs.bespitch * sh;
	  regs.besb2corg = mga_src_base + baseadrofs 
			 + 3*config->frame_size + regs.bespitch * sh;
	  regs.besa1c3org = regs.besa1corg + ((regs.bespitch * sh) / 4);
	  regs.besa2c3org = regs.besa2corg + ((regs.bespitch * sh) / 4);
	  regs.besb1c3org = regs.besb1corg + ((regs.bespitch * sh) / 4);
	  regs.besb2c3org = regs.besb2corg + ((regs.bespitch * sh) / 4);
	} 
      else 
	{
      	  regs.besa1c3org = mga_src_base + baseadrofs 
			  + 			   regs.bespitch * sh;
  	  regs.besa2c3org = mga_src_base + baseadrofs 
			  + 1*config->frame_size + regs.bespitch * sh;
  	  regs.besb1c3org = mga_src_base + baseadrofs 
			  + 2*config->frame_size + regs.bespitch * sh;
  	  regs.besb2c3org = mga_src_base + baseadrofs 
			  + 3*config->frame_size + regs.bespitch * sh;
  	  regs.besa1corg = regs.besa1c3org + ((regs.bespitch * sh) / 4);
  	  regs.besa2corg = regs.besa2c3org + ((regs.bespitch * sh) / 4);
  	  regs.besb1corg = regs.besb1c3org + ((regs.bespitch * sh) / 4);
  	  regs.besb2corg = regs.besb2c3org + ((regs.bespitch * sh) / 4);
      	}
    }

  weight = ofstop * (regs.besviscal >> 2);
  weights = weight < 0 ? 1 : 0;
  regs.besv2wght   = 
  regs.besv1wght   = (weights << 16) + ((weight & 0x3FFF) << 2);
  regs.besv2srclst = 
  regs.besv1srclst = sh - 1 - (((ofstop * regs.besviscal) >> 16) & 0x03FF);
  
  mga_vid_write_regs(0);
  return 0;
}

static void
matrox_set_grkey(const vidix_grkey_t *grkey)
{
  memcpy((void*)&mga_grkey, grkey, sizeof(vidix_grkey_t));
}

static void
matrox_set_equalizer(const vidix_video_eq_t *eq)
{
  if (eq->cap & VEQ_CAP_BRIGHTNESS)
    {
      regs.beslumactl &= 0xFFFF;
      regs.beslumactl |= (eq->brightness*255/2000)<<16;
    }
  if (eq->cap & VEQ_CAP_CONTRAST)
    {
      regs.beslumactl &= 0xFFFF0000;
      regs.beslumactl |= (128+eq->contrast*255/2000)&0xFFFF;
    }
  mga_outl(BESLUMACTL, regs.beslumactl);
}

void
matrox_vid_init(con_accel_t *accel)
{
  accel->cscs_init  = matrox_video_playback;
  accel->cscs_start = matrox_start_video;
  accel->cscs_stop  = matrox_stop_video;
  accel->cscs_grkey = matrox_set_grkey;
  accel->caps      |= ACCEL_FAST_CSCS_YV12 | ACCEL_COLOR_KEY;

  if (is_g400)
    {
      accel->caps   |= ACCEL_EQ_BRIGHTNESS | ACCEL_EQ_CONTRAST;
      accel->cscs_eq = matrox_set_equalizer;
    }
}

