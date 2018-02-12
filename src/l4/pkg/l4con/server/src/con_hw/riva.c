/**
 * \file	con/server/src/con_hw/riva.c
 * \brief	Acceleration functions for nVIDIA boards
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 *  Hacked from XFree86 Sources. */

 /***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <l4/sys/types.h>
#include <l4/sys/err.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "riva.h"
#include "riva_tbl.h"

/*
 * Define supported architectures.
 */
#define NV_ARCH_03  0x03
#define NV_ARCH_04  0x04
#define NV_ARCH_10  0x10
#define NV_ARCH_20  0x20

enum riva_chips
{
  CH_RIVA_128 = 0,
  CH_RIVA_TNT,
  CH_RIVA_TNT2,
  CH_RIVA_UTNT2,	/* UTNT2 */
  CH_RIVA_VTNT2,	/* VTNT2 */
  CH_RIVA_UVTNT2,	/* VTNT2 */
  CH_RIVA_ITNT2,	/* ITNT2 */
  CH_GEFORCE_SDR,
  CH_GEFORCE_DDR,
  CH_QUADRO,
  CH_GEFORCE2_MX,
  CH_QUADRO2_MXR,
  CH_GEFORCE2_GTS,
  CH_GEFORCE2_ULTRA,
  CH_QUADRO2_PRO,
  CH_GEFORCE2_GO,
  CH_GEFORCE3,
  CH_GEFORCE3_1,
  CH_GEFORCE3_2,
  CH_QUADRO_DDC
};

/* directly indexed by riva_chips enum, above */
static struct riva_chip_info 
{
  const char *name;
  unsigned arch_rev;
} riva_chip_info[] = 
{
    { "RIVA-128", NV_ARCH_03 },
    { "RIVA-TNT", NV_ARCH_04 },
    { "RIVA-TNT2", NV_ARCH_04 },
    { "RIVA-UTNT2", NV_ARCH_04 },
    { "RIVA-VTNT2", NV_ARCH_04 },
    { "RIVA-UVTNT2", NV_ARCH_04 },
    { "RIVA-ITNT2", NV_ARCH_04 },
    { "GeForce-SDR", NV_ARCH_10},
    { "GeForce-DDR", NV_ARCH_10},
    { "Quadro", NV_ARCH_10},
    { "GeForce2-MX", NV_ARCH_10},
    { "Quadro2-MXR", NV_ARCH_10},
    { "GeForce2-GTS", NV_ARCH_10},
    { "GeForce2-ULTRA", NV_ARCH_10},
    { "Quadro2-PRO", NV_ARCH_10},
    { "GeForce2-Go", NV_ARCH_10},
    { "GeForce3", NV_ARCH_20}, 
    { "GeForce3 Ti 200", NV_ARCH_20},
    { "GeForce3 Ti 500", NV_ARCH_20},
    { "Quadro DDC", NV_ARCH_20}
};

#define PCI_VENDOR_ID_NVIDIA			0x10de
#define PCI_DEVICE_ID_NVIDIA_TNT		0x0020
#define PCI_DEVICE_ID_NVIDIA_TNT2		0x0028
#define PCI_DEVICE_ID_NVIDIA_UTNT2		0x0029
#define PCI_DEVICE_ID_NVIDIA_VTNT2		0x002C
#define PCI_DEVICE_ID_NVIDIA_UVTNT2		0x002D
#define PCI_DEVICE_ID_NVIDIA_ITNT2		0x00A0
#define PCI_DEVICE_ID_NVIDIA_GEFORCE_SDR	0x0100
#define PCI_DEVICE_ID_NVIDIA_GEFORCE_DDR	0x0101
#define PCI_DEVICE_ID_NVIDIA_QUADRO		0x0103
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX	0x0110
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX2	0x0111
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_GO	0x0112
#define PCI_DEVICE_ID_NVIDIA_QUADRO2_MXR	0x0113
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS	0x0150
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS2	0x0151
#define PCI_DEVICE_ID_NVIDIA_GEFORCE2_ULTRA	0x0152
#define PCI_DEVICE_ID_NVIDIA_QUADRO2_PRO	0x0153
#define PCI_DEVICE_ID_NVIDIA_IGEFORCE2		0x01a0
#define PCI_DEVICE_ID_NVIDIA_GEFORCE3		0x0200
#define PCI_DEVICE_ID_NVIDIA_GEFORCE3_1		0x0201
#define PCI_DEVICE_ID_NVIDIA_GEFORCE3_2		0x0202
#define PCI_DEVICE_ID_NVIDIA_QUADRO_DDC		0x0203

static const struct pci_device_id riva_pci_tbl[] __init = 
{
    { PCI_VENDOR_ID_NVIDIA_SGS, PCI_DEVICE_ID_NVIDIA_SGS_RIVA128,
      0, 0, CH_RIVA_128 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_TNT,
      0, 0, CH_RIVA_TNT },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_TNT2,
      0, 0, CH_RIVA_TNT2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_UTNT2,
      0, 0, CH_RIVA_UTNT2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_VTNT2,
      0, 0, CH_RIVA_VTNT2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_UVTNT2,
      0, 0, CH_RIVA_VTNT2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_ITNT2,
      0, 0, CH_RIVA_ITNT2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE_SDR,
      0, 0, CH_GEFORCE_SDR },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE_DDR,
      0, 0, CH_GEFORCE_DDR },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO,
      0, 0, CH_QUADRO },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX,
      0, 0, CH_GEFORCE2_MX },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_MX2,
      0, 0, CH_GEFORCE2_MX },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO2_MXR,
      0, 0, CH_QUADRO2_MXR },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS,
      0, 0, CH_GEFORCE2_GTS },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GTS2,
      0, 0, CH_GEFORCE2_GTS },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_ULTRA,
      0, 0, CH_GEFORCE2_ULTRA },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO2_PRO,
      0, 0, CH_QUADRO2_PRO },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE2_GO,
      0, 0, CH_GEFORCE2_GO },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3,
      0, 0, CH_GEFORCE3 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3_1,
      0, 0, CH_GEFORCE3_1 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_GEFORCE3_2,
      0, 0, CH_GEFORCE3_2 },
    { PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_QUADRO_DDC,
      0, 0, CH_QUADRO_DDC },
    { 0, 0, 0, 0, 0 }
};

/*
 * Typedefs to force certain sized values.
 */
typedef unsigned char  U008;
typedef unsigned short U016;
typedef unsigned int   U032;

/*
 * HW access macros.
 */
#define NV_WR08(p,i,d)  (((U008 *)(p))[i]=(d))
#define NV_RD08(p,i)    (((U008 *)(p))[i])
#define NV_WR16(p,i,d)  (((U016 *)(p))[(i)/2]=(d))
#define NV_RD16(p,i)    (((U016 *)(p))[(i)/2])
#define NV_WR32(p,i,d)  (((U032 *)(p))[(i)/4]=(d))
#define NV_RD32(p,i)    (((U032 *)(p))[(i)/4])
#define VGA_WR08(p,i,d) NV_WR08(p,i,d)
#define VGA_RD08(p,i)   NV_RD08(p,i)


/*
 * Raster OPeration. Windows style ROP3.
 */
typedef volatile struct
{
  U032 reserved00[4];
  U016 FifoFree;
  U016 Nop;
  U032 reserved01[0x0BB];
  U032 Rop3;
} RivaRop;

/*
 * 8X8 Monochrome pattern.
 */
typedef volatile struct
{
  U032 reserved00[4];
  U016 FifoFree;
  U016 Nop;
  U032 reserved01[0x0BD];
  U032 Shape;
  U032 reserved03[0x001];
  U032 Color0;
  U032 Color1;
  U032 Monochrome[2];
} RivaPattern;

/*
 * Scissor clip rectangle.
 */
typedef volatile struct
{
  U032 reserved00[4];
  U016 FifoFree;
  U016 Nop;
  U032 reserved01[0x0BB];
  U032 TopLeft;
  U032 WidthHeight;
} RivaClip;

/*
 * 2D screen-screen BLT.
 */
typedef volatile struct
{
  U032 reserved00[4];
  U016 FifoFree;
  U016 Nop;
  U032 reserved01[0x0BB];
  U032 TopLeftSrc;
  U032 TopLeftDst;
  U032 WidthHeight;
} RivaScreenBlt;

typedef volatile struct
{
  U032 reserved00[4];
  U016 FifoFree;
  U016 Nop;
  U032 reserved01[0x0BB];
  U032 reserved03[(0x040)-1];
  U032 Color1A;
  struct
    {
      U032 TopLeft;
      U032 WidthHeight;
    } UnclippedRectangle[64];
  U032 reserved04[(0x080)-3];
  struct
    {
      U032 TopLeft;
      U032 BottomRight;
    } ClipB;
  U032 Color1B;
  struct
    {
      U032 TopLeft;
      U032 BottomRight;
    } ClippedRectangle[64];
  U032 reserved05[(0x080)-5];
  struct
    {
      U032 TopLeft;
      U032 BottomRight;
    } ClipC;
  U032 Color1C;
  U032 WidthHeightC;
  U032 PointC;
  U032 MonochromeData1C;
  U032 reserved06[(0x080)+121];
  struct
    {
      U032 TopLeft;
      U032 BottomRight;
    } ClipD;
  U032 Color1D;
  U032 WidthHeightInD;
  U032 WidthHeightOutD;
  U032 PointD;
  U032 MonochromeData1D;
  U032 reserved07[(0x080)+120];
  struct
    {
      U032 TopLeft;
      U032 BottomRight;
    } ClipE;
  U032 Color0E;
  U032 Color1E;
  U032 WidthHeightInE;
  U032 WidthHeightOutE;
  U032 PointE;
  U032 MonochromeData01E;
} RivaBitmap;

typedef struct _riva_hw_inst
{
  /*
   * Chip specific settings.
   */
  U032 Architecture;
  U032 CrystalFreqKHz;
  U032 RamAmountKBytes;
  U032 EnableIRQ;
  U032 IO;
  U032 VBlankBit;
  U032 FifoFreeCount;
  U032 FifoEmptyCount;
  /*
   * Non-FIFO registers.
   */
  volatile U032 *PCRTC;
  volatile U032 *PRAMDAC;
  volatile U032 *PFB;
  volatile U032 *PFIFO;
  volatile U032 *PGRAPH;
  volatile U032 *PEXTDEV;
  volatile U032 *PTIMER;
  volatile U032 *PMC;
  volatile U032 *PRAMIN;
  volatile U032 *FIFO;
  volatile U032 *VBLANKENABLE;
  volatile U032 *VBLANK;
  volatile U008 *PCIO;
  volatile U008 *PVIO;
  volatile U008 *PDIO;
  /*
   * Common chip functions.
   */
  int  (*Busy)(void);
  void (*SetStartAddress)(U032);
  void (*LockUnlock)(int);
  /*
   * Current extended mode settings.
   */
  /*
   * FIFO registers.
   */
  RivaRop                 *Rop;
  RivaPattern             *Patt;
  RivaClip                *Clip;
  RivaScreenBlt           *Blt;
  RivaBitmap              *Bitmap;
} RIVA_HW_INST;

static RIVA_HW_INST riva_hw;

/*
 * Extended mode state information.
 */
typedef struct _riva_hw_state
{
  U032 repaint0;
  U032 repaint1;
  U032 general;
  U032 config;
  U032 offset0;
  U032 offset1;
  U032 offset2;
  U032 offset3;
  U032 pitch0;
  U032 pitch1;
  U032 pitch2;
  U032 pitch3;
} RIVA_HW_STATE;

static RIVA_HW_STATE riva_state;

#define RIVA_FIFO_FREE(hwinst,hwptr,cnt)                           \
{                                                                  \
   while ((hwinst).FifoFreeCount < (cnt))                          \
	(hwinst).FifoFreeCount = (hwinst).hwptr->FifoFree >> 2;    \
   (hwinst).FifoFreeCount -= (cnt);                                \
}

static inline unsigned char
MISCin(void)
{
  return (VGA_RD08(riva_hw.PVIO, 0x3cc));
}

static inline void
CRTCout(unsigned char index, unsigned char val)
{
  VGA_WR08(riva_hw.PCIO, 0x3d4, index);
  VGA_WR08(riva_hw.PCIO, 0x3d5, val);
}

static inline unsigned char
CRTCin(unsigned char index)
{
  VGA_WR08(riva_hw.PCIO, 0x3d4, index);
  return (VGA_RD08(riva_hw.PCIO, 0x3d5));
}

static int
nv3Busy(void)
{
  return (   (riva_hw.Rop->FifoFree < riva_hw.FifoEmptyCount) 
          || (riva_hw.PGRAPH[0x000006B0/4] & 0x01));
}

static int
nv4Busy(void)
{
  return (   (riva_hw.Rop->FifoFree < riva_hw.FifoEmptyCount) 
	  || (riva_hw.PGRAPH[0x00000700/4] & 0x01));
}

static int
nv10Busy(void)
{
  return (   (riva_hw.Rop->FifoFree < riva_hw.FifoEmptyCount) 
	  || (riva_hw.PGRAPH[0x00000700/4] & 0x01));
}

static void
nv3LockUnlock(int LockUnlock)
{
  VGA_WR08(riva_hw.PVIO, 0x3C4, 0x06);
  VGA_WR08(riva_hw.PVIO, 0x3C5, LockUnlock ? 0x99 : 0x57);
}

static void
nv4LockUnlock(int LockUnlock)
{
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x1F);
  VGA_WR08(riva_hw.PCIO, 0x3D5, LockUnlock ? 0x99 : 0x57);
}

static void
nv10LockUnlock(int LockUnlock)
{
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x1F);
  VGA_WR08(riva_hw.PCIO, 0x3D5, LockUnlock ? 0x99 : 0x57);
}

static void
SetStartAddress(unsigned start)
{
  int offset = start >> 2;
  int pan    = (start & 3) << 1;
  unsigned char tmp;
  
  /*
   * Unlock extended registers.
   */
  riva_hw.LockUnlock(0);
  /*
   * Set start address.
   */
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x0D); 
  VGA_WR08(riva_hw.PCIO, 0x3D5, offset);
  offset >>= 8;
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x0C); 
  VGA_WR08(riva_hw.PCIO, 0x3D5, offset);
  offset >>= 8;
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x19); tmp = VGA_RD08(riva_hw.PCIO, 0x3D5);
  VGA_WR08(riva_hw.PCIO, 0x3D5, (offset & 0x01F) | (tmp & ~0x1F));
  VGA_WR08(riva_hw.PCIO, 0x3D4, 0x2D); tmp = VGA_RD08(riva_hw.PCIO, 0x3D5);
  VGA_WR08(riva_hw.PCIO, 0x3D5, (offset & 0x60) | (tmp & ~0x60));
  /*
   * 4 pixel pan register.
   */
  offset = VGA_RD08(riva_hw.PCIO, riva_hw.IO + 0x0A);
  VGA_WR08(riva_hw.PCIO, 0x3C0, 0x13);
  VGA_WR08(riva_hw.PCIO, 0x3C0, pan);
}

static void
nv3GetConfig(void)
{
  /*
   * Fill in chip configuration.
   */
  if (riva_hw.PFB[0x00000000/4] & 0x00000020)
    {
      if (   ((riva_hw.PMC[0x00000000/4] & 0xF0) == 0x20)
          && ((riva_hw.PMC[0x00000000/4] & 0x0F) >= 0x02))
	{        
	  /*
	   * SDRAM 128 ZX.
	   */
    	  switch (riva_hw.PFB[0x00000000/4] & 0x03)
    	    {
	    case 2: riva_hw.RamAmountKBytes = 1024 * 4; break;
	    case 1: riva_hw.RamAmountKBytes = 1024 * 2; break;
   	    default: riva_hw.RamAmountKBytes = 1024 * 8; break;
	    }
	}            
      else            
	{
	  riva_hw.RamAmountKBytes          = 1024 * 8;
	}            
    }
  else
    {
      /*
       * SGRAM 128.
       */
      switch (riva_hw.PFB[0x00000000/4] & 0x00000003)
	{
	case 0: riva_hw.RamAmountKBytes = 1024 * 8; break;
	case 2: riva_hw.RamAmountKBytes = 1024 * 4; break;
	default: riva_hw.RamAmountKBytes = 1024 * 2; break;
	}
    }
  
  riva_hw.VBLANKENABLE     = &(riva_hw.PGRAPH[0x0140/4]);
  riva_hw.VBLANK           = &(riva_hw.PGRAPH[0x0100/4]);
  riva_hw.VBlankBit        = 0x00000100;
  /*
   * Set chip functions.
   */
  riva_hw.Busy            = nv3Busy;
  riva_hw.SetStartAddress = SetStartAddress;
  riva_hw.LockUnlock      = nv3LockUnlock;
}

static void 
nv4GetConfig(void)
{
  /*
   * Fill in chip configuration.
   */
  if (riva_hw.PFB[0x00000000/4] & 0x00000100)
    {
      riva_hw.RamAmountKBytes 
	= ((riva_hw.PFB[0x00000000/4] >> 12) & 0x0F) * 2*1024 + 2*1024;
    }
  else
    {
      switch (riva_hw.PFB[0x00000000/4] & 0x00000003)
	{
	case 0:
	  riva_hw.RamAmountKBytes = 1024 * 32;
	  break;
	case 1:
	  riva_hw.RamAmountKBytes = 1024 * 4;
	  break;
	case 2:
	  riva_hw.RamAmountKBytes = 1024 * 8;
	  break;
	case 3:
	default:
	  riva_hw.RamAmountKBytes = 1024 * 16;
	  break;
	}
    }
  riva_hw.VBLANKENABLE     = &(riva_hw.PCRTC[0x0140/4]);
  riva_hw.VBLANK           = &(riva_hw.PCRTC[0x0100/4]);
  riva_hw.VBlankBit        = 0x00000001;
  /*
   * Set chip functions.
   */
  riva_hw.Busy            = nv4Busy;
  riva_hw.SetStartAddress = SetStartAddress;
  riva_hw.LockUnlock      = nv4LockUnlock;
}

static void
nv10GetConfig(void)
{
  /*
   * Fill in chip configuration.
   */
  switch ((riva_hw.PFB[0x0000020C/4] >> 20) & 0x000000FF)
    {
    case 0x02: riva_hw.RamAmountKBytes = 1024 * 2; break;
    case 0x04: riva_hw.RamAmountKBytes = 1024 * 4; break;
    case 0x08: riva_hw.RamAmountKBytes = 1024 * 8; break;
    case 0x10: riva_hw.RamAmountKBytes = 1024 * 16; break;
    case 0x20: riva_hw.RamAmountKBytes = 1024 * 32; break;
    case 0x40: riva_hw.RamAmountKBytes = 1024 * 64; break;
    case 0x80: riva_hw.RamAmountKBytes = 1024 * 128; break;
    default:   riva_hw.RamAmountKBytes = 1024 * 16; break;
    }
  riva_hw.VBLANKENABLE     = &(riva_hw.PCRTC[0x0140/4]);
  riva_hw.VBLANK           = &(riva_hw.PCRTC[0x0100/4]);
  riva_hw.VBlankBit        = 0x00000001;
  /*
   * Set chip functions.
   */
  riva_hw.Busy            = nv10Busy;
  riva_hw.SetStartAddress = SetStartAddress;
  riva_hw.LockUnlock      = nv10LockUnlock;
}

static int
RivaGetConfig(void)
{
  /*
   * Chip specific configuration.
   */
  switch (riva_hw.Architecture)
    {
    case NV_ARCH_03:
      nv3GetConfig();
      break;
    case NV_ARCH_04:
      nv4GetConfig();
      break;
    case NV_ARCH_10:
    case NV_ARCH_20:
      nv10GetConfig();
      break;
    default:
      return (-1);
    }
  
  /*
   * Fill in FIFO pointers.
   */
  riva_hw.Rop    = (RivaRop      *)&(riva_hw.FIFO[0x00000000/4]);
  riva_hw.Clip   = (RivaClip     *)&(riva_hw.FIFO[0x00002000/4]);
  riva_hw.Patt   = (RivaPattern  *)&(riva_hw.FIFO[0x00004000/4]);
  riva_hw.Blt    = (RivaScreenBlt*)&(riva_hw.FIFO[0x00008000/4]);
  riva_hw.Bitmap = (RivaBitmap   *)&(riva_hw.FIFO[0x0000A000/4]);
  
  return 0;
}

/*
 * Load fixed function state and pre-calculated/stored state.
 */
#define LOAD_FIXED_STATE(tbl,dev)					\
  for (i = 0; i < sizeof(tbl##Table##dev)/8; i++)			\
    riva_hw.dev[tbl##Table##dev[i][0]] = tbl##Table##dev[i][1]
#define LOAD_FIXED_STATE_8BPP(tbl,dev)					\
  for (i = 0; i < sizeof(tbl##Table##dev##_8BPP)/8; i++)		\
    riva_hw.dev[tbl##Table##dev##_8BPP[i][0]] = tbl##Table##dev##_8BPP[i][1]
#define LOAD_FIXED_STATE_15BPP(tbl,dev)					\
  for (i = 0; i < sizeof(tbl##Table##dev##_15BPP)/8; i++)		\
    riva_hw.dev[tbl##Table##dev##_15BPP[i][0]] = tbl##Table##dev##_15BPP[i][1]
#define LOAD_FIXED_STATE_16BPP(tbl,dev)					\
  for (i = 0; i < sizeof(tbl##Table##dev##_16BPP)/8; i++)		\
    riva_hw.dev[tbl##Table##dev##_16BPP[i][0]] = tbl##Table##dev##_16BPP[i][1]
#define LOAD_FIXED_STATE_32BPP(tbl,dev)					\
  for (i = 0; i < sizeof(tbl##Table##dev##_32BPP)/8; i++)		\
    riva_hw.dev[tbl##Table##dev##_32BPP[i][0]] = tbl##Table##dev##_32BPP[i][1]

static void
UpdateFifoState(void)
{
  unsigned i;
  
  switch (riva_hw.Architecture)
    {
    case NV_ARCH_04:
      LOAD_FIXED_STATE(nv4,FIFO);
      break;
    case NV_ARCH_10:
    case NV_ARCH_20:
      LOAD_FIXED_STATE(nv10,FIFO);
      break;
    }
}

static void
CalcStateExt(void)
{
  int pixelDepth = (hw_bits + 1)/8;

  switch (riva_hw.Architecture)
    {
    case NV_ARCH_03:
      riva_state.config   = ((hw_xres + 31)/32)
		          | (((pixelDepth > 2) ? 3 : pixelDepth) << 8)
		          | 0x1000;
      riva_state.general  = 0x00100100;
      riva_state.repaint1 = hw_xres < 1280 ? 0x06 : 0x02;
      break;
    case NV_ARCH_04:
      riva_state.config   = 0x00001114;
      riva_state.general  = hw_bits == 16 ? 0x00101100 : 0x00100100;
      riva_state.repaint1 = hw_xres < 1280 ? 0x04 : 0x00;
      break;
    case NV_ARCH_10:
    case NV_ARCH_20:
      riva_state.config   = riva_hw.PFB[0x00000200/4];
      riva_state.general  = hw_bits == 16 ? 0x00101100 : 0x00100100;
      riva_state.repaint1 = hw_xres < 1280 ? 0x04 : 0x00;
      break;
    }
  riva_state.repaint0 = (((hw_xres/8)*pixelDepth) & 0x700) >> 3;
  riva_state.offset0  =
  riva_state.offset1  =
  riva_state.offset2  =
  riva_state.offset3  = 0;
  riva_state.pitch0   =
  riva_state.pitch1   =
  riva_state.pitch2   =
  riva_state.pitch3   = pixelDepth * hw_xres;
}

static void
LoadStateExt(void)
{
  unsigned i;

  /*
   * Load HW fixed function state.
   */
  LOAD_FIXED_STATE(Riva,PMC);
  LOAD_FIXED_STATE(Riva,PTIMER);
  switch (riva_hw.Architecture)
    {
    case NV_ARCH_03:
      /*
       * Make sure frame buffer config gets set before loading PRAMIN.
       */
      riva_hw.PFB[0x00000200/4] = ((hw_xres+31)/32)
	                      | (((hw_bits > 2) ? 3 : hw_bits ) << 8)
			      | 0x1000;
      LOAD_FIXED_STATE(nv3,PFIFO);
      LOAD_FIXED_STATE(nv3,PRAMIN);
      LOAD_FIXED_STATE(nv3,PGRAPH);
      switch (hw_bits)
	{
	case 16:
	  printf("Riva 128 chipset doesn't support depth 16, using depth 15\n");
	  hw_bits = 15;
	case 15:
	  LOAD_FIXED_STATE_15BPP(nv3,PRAMIN);
	  LOAD_FIXED_STATE_15BPP(nv3,PGRAPH);
	  break;
	case 24:
	case 32:
	  LOAD_FIXED_STATE_32BPP(nv3,PRAMIN);
	  LOAD_FIXED_STATE_32BPP(nv3,PGRAPH);
	  break;
	case 8:
	default:
	  LOAD_FIXED_STATE_8BPP(nv3,PRAMIN);
	  LOAD_FIXED_STATE_8BPP(nv3,PGRAPH);
	  break;
	}
      for (i = 0x00000; i < 0x00800; i++)
	riva_hw.PRAMIN[0x00000502 + i] = (i << 12) | 0x03;
      riva_hw.PGRAPH[0x00000630/4] = riva_state.offset0;
      riva_hw.PGRAPH[0x00000634/4] = riva_state.offset1;
      riva_hw.PGRAPH[0x00000638/4] = riva_state.offset2;
      riva_hw.PGRAPH[0x0000063C/4] = riva_state.offset3;
      riva_hw.PGRAPH[0x00000650/4] = riva_state.pitch0;
      riva_hw.PGRAPH[0x00000654/4] = riva_state.pitch1;
      riva_hw.PGRAPH[0x00000658/4] = riva_state.pitch2;
      riva_hw.PGRAPH[0x0000065C/4] = riva_state.pitch3;
      break;
    case NV_ARCH_04:
      /*
       * Make sure frame buffer config gets set before loading PRAMIN.
       */
      riva_hw.PFB[0x00000200/4] = 0x00001114;
      LOAD_FIXED_STATE(nv4,PFIFO);
      LOAD_FIXED_STATE(nv4,PRAMIN);
      LOAD_FIXED_STATE(nv4,PGRAPH);
      switch (hw_bits)
	{
	case 15:
	  LOAD_FIXED_STATE_15BPP(nv4,PRAMIN);
	  LOAD_FIXED_STATE_15BPP(nv4,PGRAPH);
	  break;
	case 16:
	  LOAD_FIXED_STATE_16BPP(nv4,PRAMIN);
	  LOAD_FIXED_STATE_16BPP(nv4,PGRAPH);
	  break;
	case 24:
	case 32:
	  LOAD_FIXED_STATE_32BPP(nv4,PRAMIN);
	  LOAD_FIXED_STATE_32BPP(nv4,PGRAPH);
	  break;
	case 8:
	default:
	  LOAD_FIXED_STATE_8BPP(nv4,PRAMIN);
	  LOAD_FIXED_STATE_8BPP(nv4,PGRAPH);
	  break;
	}
      riva_hw.PGRAPH[0x00000640/4] = riva_state.offset0;
      riva_hw.PGRAPH[0x00000644/4] = riva_state.offset1;
      riva_hw.PGRAPH[0x00000648/4] = riva_state.offset2;
      riva_hw.PGRAPH[0x0000064C/4] = riva_state.offset3;
      riva_hw.PGRAPH[0x00000670/4] = riva_state.pitch0;
      riva_hw.PGRAPH[0x00000674/4] = riva_state.pitch1;
      riva_hw.PGRAPH[0x00000678/4] = riva_state.pitch2;
      riva_hw.PGRAPH[0x0000067C/4] = riva_state.pitch3;
      break;
    case NV_ARCH_10:
    case NV_ARCH_20:
      LOAD_FIXED_STATE(nv10,PFIFO);
      LOAD_FIXED_STATE(nv10,PRAMIN);
      LOAD_FIXED_STATE(nv10,PGRAPH);
      switch (hw_bits)
	{
	case 15:
	  LOAD_FIXED_STATE_15BPP(nv10,PRAMIN);
	  LOAD_FIXED_STATE_15BPP(nv10,PGRAPH);
	  break;
	case 16:
	  LOAD_FIXED_STATE_16BPP(nv10,PRAMIN);
	  LOAD_FIXED_STATE_16BPP(nv10,PGRAPH);
	  break;
	case 24:
	case 32:
	  LOAD_FIXED_STATE_32BPP(nv10,PRAMIN);
	  LOAD_FIXED_STATE_32BPP(nv10,PGRAPH);
	  break;
	case 8:
	default:
	  LOAD_FIXED_STATE_8BPP(nv10,PRAMIN);
	  LOAD_FIXED_STATE_8BPP(nv10,PGRAPH);
	  break;
	}

      if (riva_hw.Architecture == NV_ARCH_10)
	{
	  riva_hw.PGRAPH[0x00000640/4] = riva_state.offset0;
	  riva_hw.PGRAPH[0x00000644/4] = riva_state.offset1;
    	  riva_hw.PGRAPH[0x00000648/4] = riva_state.offset2;
	  riva_hw.PGRAPH[0x0000064C/4] = riva_state.offset3;
	  riva_hw.PGRAPH[0x00000670/4] = riva_state.pitch0;
	  riva_hw.PGRAPH[0x00000674/4] = riva_state.pitch1;
	  riva_hw.PGRAPH[0x00000678/4] = riva_state.pitch2;
	  riva_hw.PGRAPH[0x0000067C/4] = riva_state.pitch3;
	  riva_hw.PGRAPH[0x00000680/4] = riva_state.pitch3;
	} 
      else 
	{
  	  riva_hw.PGRAPH[0x00000820/4] = riva_state.offset0;
	  riva_hw.PGRAPH[0x00000824/4] = riva_state.offset1;
	  riva_hw.PGRAPH[0x00000828/4] = riva_state.offset2;
	  riva_hw.PGRAPH[0x0000082C/4] = riva_state.offset3;
	  riva_hw.PGRAPH[0x00000850/4] = riva_state.pitch0;
	  riva_hw.PGRAPH[0x00000854/4] = riva_state.pitch1;
	  riva_hw.PGRAPH[0x00000858/4] = riva_state.pitch2;
	  riva_hw.PGRAPH[0x0000085C/4] = riva_state.pitch3;
	  riva_hw.PGRAPH[0x00000860/4] = riva_state.pitch3;
	  riva_hw.PGRAPH[0x00000864/4] = riva_state.pitch3;
	  riva_hw.PGRAPH[0x000009A4/4] = riva_hw.PFB[0x00000200/4];
	  riva_hw.PGRAPH[0x000009A8/4] = riva_hw.PFB[0x00000204/4];
	}
      break;
    }
  LOAD_FIXED_STATE(Riva,FIFO);
  UpdateFifoState();
  /*
   * Load HW mode state.
   */
  VGA_WR08(riva_hw.PCIO, 0x03D4, 0x19);
  VGA_WR08(riva_hw.PCIO, 0x03D5, riva_state.repaint0);
  VGA_WR08(riva_hw.PCIO, 0x03D4, 0x1A);
  VGA_WR08(riva_hw.PCIO, 0x03D5, riva_state.repaint1);
  riva_hw.PRAMDAC[0x00000600/4]  = riva_state.general;
  /*
   * Turn off VBlank enable and reset.
   */
  *(riva_hw.VBLANKENABLE) = 0;
  *(riva_hw.VBLANK)       = riva_hw.VBlankBit;
  /*
   * Set interrupt enable.
   */    
  riva_hw.PMC[0x00000140/4]  = riva_hw.EnableIRQ & 0x01;
  /*
   * Reset FIFO free and empty counts.
   */
  riva_hw.FifoFreeCount  = 0;
  /* Free count from first subchannel */
  riva_hw.FifoEmptyCount = riva_hw.Rop->FifoFree;
}

static inline void
wait_for_idle(void)
{
  while (riva_hw.Busy())
    ;
}

/* exported: wait until graphics engine is idle */
static void
riva_sync(void)
{
  wait_for_idle();
}

/* exported: accelerated copy */
static void
riva_bmove(struct l4con_vc *vc, 
	   int sx, int sy, int width, int height, int dx, int dy)
{
  (void)vc;
  RIVA_FIFO_FREE(riva_hw, Blt, 3);
  riva_hw.Blt->TopLeftSrc  = (sy << 16) | sx;
  riva_hw.Blt->TopLeftDst  = (dy << 16) | dx;
  riva_hw.Blt->WidthHeight = (height  << 16) | width;
}

/* exported: accelerated fill */
static void 
riva_fill(struct l4con_vc *vc, 
	  int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  RIVA_FIFO_FREE(riva_hw, Bitmap, 1);
  riva_hw.Bitmap->Color1A = color;
  
  RIVA_FIFO_FREE(riva_hw, Bitmap, 2);
  riva_hw.Bitmap->UnclippedRectangle[0].TopLeft     = (sx << 16) | sy; 
  riva_hw.Bitmap->UnclippedRectangle[0].WidthHeight = (width << 16) | height;
}

/* exported: set display start address */
static void
riva_pan(int *x, int *y)
{
  unsigned bpp = (hw_bits + 1) / 8;
 
  SetStartAddress((*y * hw_xres + *x) * bpp);
}

/* set copy ROP, no mask */
static void
riva_setup_ROP(void)
{
  RIVA_FIFO_FREE(riva_hw, Patt, 5);
  riva_hw.Patt->Shape = 0;
  riva_hw.Patt->Color0 = 0xffffffff;
  riva_hw.Patt->Color1 = 0xffffffff;
  riva_hw.Patt->Monochrome[0] = 0xffffffff;
  riva_hw.Patt->Monochrome[1] = 0xffffffff;

  RIVA_FIFO_FREE(riva_hw, Rop, 1);
  riva_hw.Rop->Rop3 = 0xCC;
}

static void
riva_setup_accel(void)
{
  RIVA_FIFO_FREE(riva_hw, Clip, 2);
  riva_hw.Clip->TopLeft     = 0x0;
  riva_hw.Clip->WidthHeight = 0x80008000;
  riva_setup_ROP();
  wait_for_idle();
}

static int
riva_probe(unsigned int bus, unsigned int devfn, 
	   const struct pci_device_id *device,  con_accel_t *accel)
{
  l4_addr_t addr0, addr1;
  l4_size_t size0, size1;
  struct riva_chip_info *rci;
  l4_addr_t ctrl_base;
  unsigned char rev;

  PCIBIOS_READ_CONFIG_BYTE (bus, devfn, PCI_REVISION_ID, &rev);
  rci = &riva_chip_info[device->driver_data];

  pci_resource(bus, devfn, 0, &addr0, &size0);
  pci_resource(bus, devfn, 1, &addr1, &size1);

  riva_hw.Architecture = rci->arch_rev;

  if ((size0 < 0x00800000) || (size1 < 0x01000000))
    {
      printf("Riva: Invalid size\n");
      return -L4_ENODEV;
    }

  /* map memory mapped i/o registers */
  if (map_io_mem(addr0, size0, 0, "ctrl", &ctrl_base)<0)
    return -L4_ENODEV;

  if (addr1 != hw_vid_mem_addr)
    {
      printf("addr1 != hw_vid_mem_addr!\n");
      return -L4_ENODEV;
    }

  if (size1 < hw_vid_mem_size)
    size1 = hw_vid_mem_size;

  if (map_io_mem(hw_vid_mem_addr, size1, 1, "video",
	         (l4_addr_t *)&hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  riva_hw.EnableIRQ = 0;
  riva_hw.PRAMDAC   = (unsigned *)(ctrl_base + 0x00680000);
  riva_hw.PFB       = (unsigned *)(ctrl_base + 0x00100000);
  riva_hw.PFIFO     = (unsigned *)(ctrl_base + 0x00002000);
  riva_hw.PGRAPH    = (unsigned *)(ctrl_base + 0x00400000);
  riva_hw.PEXTDEV   = (unsigned *)(ctrl_base + 0x00101000);
  riva_hw.PTIMER    = (unsigned *)(ctrl_base + 0x00009000);
  riva_hw.PMC       = (unsigned *)(ctrl_base + 0x00000000);
  riva_hw.FIFO      = (unsigned *)(ctrl_base + 0x00800000);

  riva_hw.PCIO      = (U008 *)(ctrl_base + 0x00601000);
  riva_hw.PDIO      = (U008 *)(ctrl_base + 0x00681000);
  riva_hw.PVIO      = (U008 *)(ctrl_base + 0x000C0000);

  riva_hw.IO        = (MISCin() & 0x01) ? 0x3D0 : 0x3B0;

  switch (riva_hw.Architecture) 
    {
    case NV_ARCH_03:
      riva_hw.PRAMIN = (unsigned *)(hw_map_vid_mem_addr + 0x00C00000);
      break;
    case NV_ARCH_04:
    case NV_ARCH_10:
    case NV_ARCH_20:
      riva_hw.PCRTC  = (unsigned *)(ctrl_base + 0x00600000);
      riva_hw.PRAMIN = (unsigned *)(ctrl_base + 0x00710000);
      break;
    }
	      
  RivaGetConfig();

  /* unlock io */
  CRTCout(0x11, 0xFF);
  riva_hw.LockUnlock(0);

  printf("Found nVidia NV%d (%s, %dMB) rev 0x%02x (PCI %02x/%02x)\n",
      riva_hw.Architecture, rci->name, riva_hw.RamAmountKBytes / 1024,
      rev, bus, devfn);

  CalcStateExt();
  LoadStateExt();
  riva_setup_accel();

  accel->copy = riva_bmove;
  accel->fill = riva_fill;
  accel->sync = riva_sync;
  accel->pan  = riva_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  return 0;
}

void
riva_register(void)
{
  pci_register(riva_pci_tbl, riva_probe);
}

