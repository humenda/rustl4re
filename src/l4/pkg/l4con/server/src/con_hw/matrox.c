/*!
 * \file	matrox.c
 * \brief	Hardware Acceleration for Matrox Gxx cards
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

/* most stuff taken from Linux 2.2.21: drivers/video/matroxfb.c */

#include <stdio.h>

#include <l4/sys/err.h>
#include <l4/sys/ipc.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "matrox.h"
#include "matrox_regs.h"

int is_g400 = 0;

static unsigned m_dwg_rect = 0;
static unsigned m_opmode = 0;
l4_addr_t mga_mmio_vbase = 0;
static unsigned matrox_pci_bus_id;
static unsigned matrox_pci_devfn_id;

#define M_DWGCTL	0x1C00
#define M_MACCESS	0x1C04
#define M_CTLWTST	0x1C08

#define M_PLNWT		0x1C1C

#define M_BCOL		0x1C20
#define M_FCOL		0x1C24

#define M_SGN		0x1C58
#define M_LEN		0x1C5C
#define M_AR0		0x1C60
#define M_AR1		0x1C64
#define M_AR2		0x1C68
#define M_AR3		0x1C6C
#define M_AR4		0x1C70
#define M_AR5		0x1C74
#define M_AR6		0x1C78

#define M_CXBNDRY	0x1C80
#define M_FXBNDRY	0x1C84
#define M_YDSTLEN	0x1C88
#define M_PITCH		0x1C8C
#define M_YDST		0x1C90
#define M_YDSTORG	0x1C94
#define M_YTOP		0x1C98
#define M_YBOT		0x1C9C

#define M_CRTC_INDEX	0x1FD4
#define M_EXTVGA_INDEX	0x1FDE

#define M_EXEC          0x0100

#define M_DWG_TRAP	0x04
#define M_DWG_BITBLT	0x08
#define M_DWG_ILOAD	0x09

#define M_DWG_LINEAR	0x0080
#define M_DWG_SOLID	0x0800
#define M_DWG_ARZERO	0x1000
#define M_DWG_SGNZERO	0x2000
#define M_DWG_SHIFTZERO	0x4000

#define M_DWG_REPLACE	0x000C0000
#define M_DWG_REPLACE2	(M_DWG_REPLACE | 0x40)
#define M_DWG_XOR	0x00060010

#define M_DWG_BFCOL	0x04000000
#define M_DWG_BMONOWF	0x08000000

#define M_DWG_TRANSC	0x40000000

#define M_FIFOSTATUS    0x1E10
#define M_STATUS        0x1E14

#define mga_ydstlen(y,l) mga_outl(M_YDSTLEN | M_EXEC, ((y) << 16) | (l))


#define FB_ACCEL_MATROX_MGA2064W 16	/* Matrox MGA2064W (Millenium)	*/
#define FB_ACCEL_MATROX_MGA1064SG 17	/* Matrox MGA1064SG (Mystique)	*/
#define FB_ACCEL_MATROX_MGA2164W 18	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGA2164W_AGP 19	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGAG100	20	/* Matrox G100 (Productiva G100) */
#define FB_ACCEL_MATROX_MGAG200	21	/* Matrox G200 (Myst, Mill, ...) */
#define FB_ACCEL_MATROX_MGAG400 26	/* Matrox G400                   */

#define PCI_SS_VENDOR_ID_SIEMENS_NIXDORF	0x110A
#define PCI_SS_VENDOR_ID_MATROX		PCI_VENDOR_ID_MATROX
#define PCI_DEVICE_ID_MATROX_G200_PCI		0x0520
#define PCI_DEVICE_ID_MATROX_G200_AGP		0x0521
#define PCI_DEVICE_ID_MATROX_G100		0x1000
#define PCI_DEVICE_ID_MATROX_G100_AGP		0x1001
#define PCI_DEVICE_ID_MATROX_G400_AGP		0x0525
#define PCI_DEVICE_ID_MATROX_G550_AGP		0x2527
#define PCI_SS_ID_MATROX_GENERIC		0xFF00
#define PCI_SS_ID_MATROX_PRODUCTIVA_G100_AGP	0xFF01
#define PCI_SS_ID_MATROX_MYSTIQUE_G200_AGP	0xFF02
#define PCI_SS_ID_MATROX_MILLENIUM_G200_AGP	0xFF03
#define PCI_SS_ID_MATROX_MARVEL_G200_AGP	0xFF04
#define PCI_SS_ID_MATROX_MGA_G100_PCI		0xFF05
#define PCI_SS_ID_MATROX_MGA_G100_AGP		0x1001
#define PCI_SS_ID_MATROX_MILLENNIUM_G400_MAX_AGP 0x2179
#define PCI_SS_ID_SIEMENS_MGA_G100_AGP		0x001E /* 30 */
#define PCI_SS_ID_SIEMENS_MGA_G200_AGP		0x0032 /* 50 */


#define M_OPMODE 			0x1E54

#define M_OPMODE_DMA_GEN_WRITE		0x00
#define M_OPMODE_DMA_BLIT		0x04
#define M_OPMODE_DMA_VECTOR_WRITE	0x08
#define M_OPMODE_DMA_LE			0x0000
#define M_OPMODE_DMA_BE_8BPP		0x0000
#define M_OPMODE_DMA_BE_16BPP		0x0100
#define M_OPMODE_DMA_BE_32BPP		0x0200
#define M_OPMODE_DIR_LE			0x000000
#define M_OPMODE_DIR_BE_8BPP		0x000000
#define M_OPMODE_DIR_BE_16BPP		0x010000
#define M_OPMODE_DIR_BE_32BPP		0x020000

#define M_OPMODE_8BPP	(M_OPMODE_DMA_LE | M_OPMODE_DIR_LE | M_OPMODE_DMA_BLIT)
#define M_OPMODE_16BPP	(M_OPMODE_DMA_LE | M_OPMODE_DIR_LE | M_OPMODE_DMA_BLIT)
#define M_OPMODE_24BPP	(M_OPMODE_DMA_LE | M_OPMODE_DIR_LE | M_OPMODE_DMA_BLIT)
#define M_OPMODE_32BPP	(M_OPMODE_DMA_LE | M_OPMODE_DIR_LE | M_OPMODE_DMA_BLIT)

struct video_board 
{
  int maxvram;
  int maxdisplayable;
  int accelID;
};

static struct video_board vbMillenium = 
  {0x0800000, 0x0800000, FB_ACCEL_MATROX_MGA2064W};
static struct video_board vbMillenium2 = 
  {0x1000000, 0x0800000, FB_ACCEL_MATROX_MGA2164W};
static struct video_board vbMillenium2A = 
  {0x1000000, 0x0800000, FB_ACCEL_MATROX_MGA2164W_AGP};
static struct video_board vbMystique = 
  {0x0800000, 0x0800000, FB_ACCEL_MATROX_MGA1064SG};
static struct video_board vbG100 = 
  {0x0800000, 0x0800000, FB_ACCEL_MATROX_MGAG100};
static struct video_board vbG200 = 
  {0x1000000, 0x1000000, FB_ACCEL_MATROX_MGAG200};
static struct video_board vbG400 = 
  {0x2000000, 0x1000000, FB_ACCEL_MATROX_MGAG400};

#define DEVF_VIDEO64BIT		0x0001
#define	DEVF_SWAPS		0x0002
#define DEVF_MILLENIUM		0x0004
#define	DEVF_MILLENIUM2		0x0008
#define DEVF_CROSS4MB		0x0010
#define DEVF_TEXT4B		0x0020
#define DEVF_DDC_8_2		0x0040
#define DEVF_DMA		0x0080
#define DEVF_SUPPORT32MB	0x0100
#define DEVF_ANY_VXRES		0x0200
#define DEVF_TEXT16B		0x0400
#define DEVF_CRTC2		0x0800
#define DEVF_MAVEN_CAPABLE	0x1000
#define DEVF_PANELLINK_CAPABLE	0x2000
#define DEVF_G450DAC		0x4000
#define DEVF_BES3PLANE	    0x80000000 /* backend scaler can 420 */


#define DEVF_G100	(DEVF_VIDEO64BIT | DEVF_SWAPS |\
			 DEVF_CROSS4MB | DEVF_DDC_8_2)
#define DEVF_G200	(DEVF_VIDEO64BIT | DEVF_SWAPS |\
			 DEVF_CROSS4MB | DEVF_DDC_8_2 | DEVF_ANY_VXRES)
#define DEVF_G400	(DEVF_VIDEO64BIT | DEVF_SWAPS |\
			 DEVF_CROSS4MB | DEVF_DDC_8_2 |\
			 DEVF_ANY_VXRES | DEVF_SUPPORT32MB |\
			 DEVF_TEXT16B | DEVF_BES3PLANE)
#define DEVF_G450	(DEVF_VIDEO64BIT | DEVF_SWAPS |\
			 DEVF_CROSS4MB | DEVF_DDC_8_2 |\
			 DEVF_ANY_VXRES | DEVF_SUPPORT32MB |\
			 DEVF_TEXT16B | DEVF_CRTC2 | DEVF_G450DAC |\
			 DEVF_MILLENIUM | DEVF_BES3PLANE)
#define DEVF_G550	(DEVF_G450 | DEVF_DMA | DEVF_MILLENIUM2)

struct board_t
{
  unsigned int rev;
  unsigned int flags;
  unsigned int maxclk;
  struct video_board* base;
  const char* name;
};

enum matrox_chip
{
  CH_MILLENNIUM = 0,
  CH_MILLENNIUM_II,
  CH_MILLENNIUM_II_AGP,
  CH_MYSTIQUE,
  CH_MYSTIQUE_220,
  CH_G100_PCI,
  CH_G100_PCI_UNKNOWN,
  CH_G100_AGP,
  CH_G100_PRODUCTIVA,
  CH_G100_AGP_UNKNOWN,
  CH_G200_PCI_UNKNOWN,
  CH_G200_AGP,
  CH_G200_MYSTIQUE,
  CH_G200_MILLENNIUM,
  CH_G200_MARVEL,
  CH_G400_MAX,
  CH_G400_AGP,
  CH_G450_AGP,
  CH_G550_AGP
};

static struct board_t matrox_boards[] =
{
    { 0xFF, DEVF_MILLENIUM | DEVF_TEXT4B, 230000,
      &vbMillenium, "Millennium (PCI)"},
    { 0xFF, DEVF_MILLENIUM | DEVF_MILLENIUM2 | DEVF_SWAPS, 220000,
      &vbMillenium2, "Millennium II (PCI)"},
    { 0xFF, DEVF_MILLENIUM | DEVF_MILLENIUM2 | DEVF_SWAPS, 250000,
      &vbMillenium2A, "Millennium II (AGP)"},
    { 0x02, DEVF_VIDEO64BIT, 180000,
      &vbMystique, "Mystique (PCI)"},
    { 0xFF, DEVF_VIDEO64BIT | DEVF_SWAPS, 220000,
      &vbMystique, "Mystique 220 (PCI)"},
    { 0xFF, DEVF_G100, 230000,
      &vbG100, "MGA-G100 (PCI)"},
    { 0xFF, DEVF_G100, 230000,
      &vbG100, "unknown G100 (PCI)"},
    { 0xFF, DEVF_G100, 230000,
      &vbG100, "MGA-G100 (AGP)"},
    { 0xFF, DEVF_G100, 230000,
      &vbG100, "Productiva G100 (AGP)"},
    { 0xFF, DEVF_G100, 230000,
      &vbG100, "unknown G100 (AGP)"},
    { 0xFF, DEVF_G200, 250000,
      &vbG200, "unknown G200 (PCI)"},
    { 0xFF, DEVF_G200, 220000,
      &vbG200, "MGA-G200 (AGP)"},
    { 0xFF, DEVF_G200, 230000,
      &vbG200, "Mystique G200 (AGP)"},
    { 0xFF, DEVF_G200, 250000,
      &vbG200, "Millennium G200 (AGP)"},
    { 0xFF, DEVF_G200, 230000,
      &vbG200, "Marvel G200 (AGP)"},
    { 0x80, DEVF_G400, 360000,
      &vbG400, "Millennium G400 MAX (AGP)"},
    { 0x80, DEVF_G400, 300000,
      &vbG400, "G400 (AGP)"},
    { 0xFF, DEVF_G450, 500000,
      &vbG400, "G450"},
    { 0xFF, DEVF_G550, 500000,
      &vbG400, "G550"}
};

static const struct pci_device_id matrox_pci_tbl[] =
{
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_MIL, 0,	0,
     CH_MILLENNIUM},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_MIL_2, 0, 0, 
     CH_MILLENNIUM_II},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_MIL_2_AGP, 0, 0, 
     CH_MILLENNIUM_II_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_MYS, 0,	0,
     CH_MYSTIQUE},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_MYS, 0, 0,
     CH_MYSTIQUE_220},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_MGA_G100_PCI,
     CH_G100_PCI},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100, 0, 0,
     CH_G100_PCI_UNKNOWN},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_GENERIC,
     CH_G100_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_MGA_G100_AGP,
     CH_G100_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100_AGP,
     PCI_SS_VENDOR_ID_SIEMENS_NIXDORF, PCI_SS_ID_SIEMENS_MGA_G100_AGP,
     CH_G100_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100_AGP,
     PCI_SS_VENDOR_ID_MATROX,PCI_SS_ID_MATROX_PRODUCTIVA_G100_AGP, 
     CH_G100_PRODUCTIVA},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G100_AGP, 0, 0,
     CH_G100_AGP_UNKNOWN},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_PCI, 0, 0,
     CH_G200_PCI_UNKNOWN},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_GENERIC,
     CH_G200_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_MYSTIQUE_G200_AGP,
     CH_G200_MYSTIQUE},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_MILLENIUM_G200_AGP,
     CH_G200_MILLENNIUM},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP,
     PCI_SS_VENDOR_ID_MATROX, PCI_SS_ID_MATROX_MARVEL_G200_AGP,
     CH_G200_MARVEL},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP,
     PCI_SS_VENDOR_ID_SIEMENS_NIXDORF, PCI_SS_ID_SIEMENS_MGA_G200_AGP,
     CH_G200_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G200_AGP, 0, 0,
     CH_G200_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G400_AGP,
     PCI_SS_VENDOR_ID_MATROX,PCI_SS_ID_MATROX_MILLENNIUM_G400_MAX_AGP,
     CH_G400_MAX},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G400_AGP, 0, 0,
     CH_G400_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G400_AGP, 0, 0,
     CH_G450_AGP},
    {PCI_VENDOR_ID_MATROX, PCI_DEVICE_ID_MATROX_G550_AGP, 0, 0,
     CH_G550_AGP},
    {0, 0, 0, 0, 0}
};

static void
matrox_pan(int *x, int *y)
{
  unsigned p0, p1, p2, p3;
  unsigned bpp = (hw_bits + 1) & ~7;
  unsigned start = ((*y * hw_xres + *x) * bpp) / 64;
  
  p0 = ((start & 0x000000FF)      );
  p1 = ((start & 0x0000FF00) >>  8);
  p2 = (mga_readr(M_EXTVGA_INDEX, 0x00) & 0xB0) |
       ((start & 0x000F0000) >> 16) | ((start & 0x00100000) >> 14);
  p3 = ((start             ) >> 21);
  
  mga_setr(M_CRTC_INDEX,   0x0D, p0);
  mga_setr(M_CRTC_INDEX,   0x0C, p1);
  mga_setr(M_EXTVGA_INDEX, 0x08, p3);
  mga_setr(M_EXTVGA_INDEX, 0x00, p2);
}

static void
matrox_bmove(struct l4con_vc *vc,
	     int sx, int sy, int width, int height, int dx, int dy)
{
  int pixx = hw_xres;
  int start, end;
  (void)vc;

  if ((dy < sy) || ((dy == sy) && (dx <= sx))) 
    {
      mga_fifo(2);
      mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO | M_DWG_SGNZERO |
			 M_DWG_BFCOL | M_DWG_REPLACE);
      mga_outl(M_AR5, pixx);
      width--;
      start = sy*pixx+sx;
      end = start+width;
    } 
  else 
    {
      mga_fifo(3);
      mga_outl(M_DWGCTL, M_DWG_BITBLT | M_DWG_SHIFTZERO |
			 M_DWG_BFCOL | M_DWG_REPLACE);
      mga_outl(M_SGN, 5);
      mga_outl(M_AR5, -pixx);
      width--;
      end = (sy+height-1)*pixx+sx;
      start = end+width;
      dy += height-1;
    }
  mga_fifo(4);
  mga_outl(M_AR0, end);
  mga_outl(M_AR3, start);
  mga_outl(M_FXBNDRY, ((dx+width)<<16) | dx);
  mga_ydstlen(dy, height);
}

static void 
matrox_fill(struct l4con_vc *vc,
	    int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  mga_fifo(5);
  mga_outl(M_DWGCTL, m_dwg_rect | M_DWG_REPLACE);
  mga_outl(M_FCOL, color);
  mga_outl(M_FXBNDRY, ((sx + width) << 16) | sx);
  mga_ydstlen(sy, height);
}

#if 0
static void
matrox_blit(struct l4con_vc *vc, unsigned fgx, unsigned bgx,
	    const l4_uint8_t* chardata, int width, int height, int yy, int xx)
{
  l4_uint32_t step;
  l4_uint32_t ydstlen;
  l4_uint32_t xlen;
  l4_uint32_t ar0;
  l4_uint32_t charcell;
  l4_uint32_t fxbndry;
  int easy;

  step = (width + 7) >> 3;
  charcell = height * step;
  xlen = (charcell + 3) & ~3;
  ydstlen = (yy << 16) | height;
  if (width == step << 3)
    {
      ar0 = height * width - 1;
      easy = 1;
    }
  else
    {
      ar0 = width - 1;
      easy = 0;
    }

  mga_fifo(3);
  if (easy)
    mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | 
		       M_DWG_LINEAR | M_DWG_REPLACE);
  else
    mga_outl(M_DWGCTL, M_DWG_ILOAD | M_DWG_SGNZERO | M_DWG_SHIFTZERO | 
		       M_DWG_BFCOL | M_DWG_REPLACE);
  fxbndry = ((xx + width - 1) << 16) | xx;

  mga_fifo(6);
  mga_writel(mga_mmio_vbase, M_FXBNDRY, fxbndry);
  mga_writel(mga_mmio_vbase, M_AR0, ar0);
  mga_writel(mga_mmio_vbase, M_AR3, 0);
  if (easy)
    {
      mga_writel(mga_mmio_vbase, M_YDSTLEN | M_EXEC, ydstlen);
      memcpy((void*)mga_mmio_vbase, chardata, xlen);
    }
  else
    {
      mga_writel(mga_mmio_vbase, M_AR5, 0);
      mga_writel(mga_mmio_vbase, M_YDSTLEN | M_EXEC, ydstlen);
      if ((step & 3) == 0)
	{
	  /* Great. Source has 32bit aligned lines, so we can feed them
	     directly to the accelerator. */
	  memcpy((void*)mga_mmio_vbase, chardata, charcell);
	}
      else if (step == 1)
	{
	  /* Special case for 1..8bit widths */
	  while (height--)
	    {
	      mga_outl(0, *chardata);
	      chardata++;
	    }
	}
      else if (step == 2)
	{
	  /* Special case for 9..15bit widths */
	  while (height--)
	    {
	      mga_outl(0, *(l4_uint16_t*)chardata);
	      chardata += 2;
	    }
	}
      else
	{
	  while (height--)
	    {
	      size_t i;	      
	      for (i = 0; i < step; i += 4)
      		mga_outl(0, *(l4_uint32_t*)(chardata + i));
	      chardata += step;
	    }
	}
    }
}
#endif

/* Without I/O flexpages, we can't prohibit other L4 tasks to write to I/O
 * ports. In that case it is possible, that we start X in L4Linux which does
 * some things with the PCI configuration area of our graphics card so that
 * it's memory mapped registers are no longer mapped. Test this case here
 * and get back registers if needed */
static void
matrox_test_for_card_disappeared(void)
{
  if (mga_inl(M_STATUS) == -1U)
    {
      unsigned short tmp;

      printf("Matrox video card disappered -- re-mapping memory mapped "
	     "registers.\n");
      PCIBIOS_READ_CONFIG_WORD(matrox_pci_bus_id, matrox_pci_devfn_id,
			       PCI_COMMAND, &tmp);
      tmp |= PCI_COMMAND_MEMORY;
      PCIBIOS_WRITE_CONFIG_WORD(matrox_pci_bus_id, matrox_pci_devfn_id,
				PCI_COMMAND, tmp);
    }
}

static void
matrox_sync(void)
{
  matrox_test_for_card_disappeared();
  WaitTillIdle();
}


static void 
matrox_init(int accelID)
{
  unsigned maccess;
  unsigned mpitch;
  unsigned mopmode;

  mpitch = hw_xres;

  switch (hw_bits) 
    {
    case  8: maccess = 0x00000000; mopmode = M_OPMODE_8BPP;  break;
    case 15: maccess = 0xC0000001; mopmode = M_OPMODE_16BPP; break;
    case 16: maccess = 0x40000001; mopmode = M_OPMODE_16BPP; break;
    case 24: maccess = 0x00000003; mopmode = M_OPMODE_24BPP; break;
    case 32: maccess = 0x00000002; mopmode = M_OPMODE_32BPP; break;
    default: maccess = 0x00000000; mopmode = 0x00000000;     break;
    }
  mga_fifo(8);
  mga_outl(M_PITCH, mpitch);
  mga_outl(M_YDSTORG, 0);
  if (accelID != FB_ACCEL_MATROX_MGAG100)
    mga_outl(M_PLNWT, -1);
  mga_outl(M_OPMODE, mopmode);
  mga_outl(M_CXBNDRY, 0xFFFF0000);
  mga_outl(M_YTOP, 0);
  mga_outl(M_YBOT, 0x01FFFFFF);
  mga_outl(M_MACCESS, maccess);
  m_dwg_rect = M_DWG_TRAP | M_DWG_SOLID | M_DWG_ARZERO | M_DWG_SGNZERO 
			  | M_DWG_SHIFTZERO;
  m_opmode = mopmode;
}

static int
matrox_probe(unsigned int bus, unsigned int devfn,
	     const struct pci_device_id *dev, con_accel_t *accel)
{
  struct board_t *b = &matrox_boards[dev->driver_data];
  unsigned char rev;
  l4_addr_t addr0, addr1, ctrl_addr, video_addr;
  l4_size_t size0, size1, video_size;

  PCIBIOS_READ_CONFIG_BYTE (bus, devfn, PCI_REVISION_ID, &rev);

  if (b->rev < rev)
    return -L4_ENODEV;

  pci_resource(bus, devfn, 0, &addr0, &size0);
  pci_resource(bus, devfn, 1, &addr1, &size1);

  if (b->flags & DEVF_SWAPS)
    {
      ctrl_addr  = addr1 & ~0x00003FFF;
      video_addr = addr0 & ~0x007FFFFF;
      video_size = size0;
    }
  else
    {
      ctrl_addr  = addr0 & ~0x00003FFF;
      video_addr = addr1 & ~0x007FFFFF;
      video_size = size1;
    }

  is_g400 = b->flags & DEVF_SUPPORT32MB;

  printf("Found Matrox %s rev 0x%02x (PCI %02x/%02x)\n",
	  b->name, rev, bus, devfn);

  if (map_io_mem(ctrl_addr, 0x4000, 0, "ctrl", (l4_addr_t *)&mga_mmio_vbase)<0)
    return -L4_ENODEV;

  if (map_io_mem(video_addr, video_size, 1, "video",
	         (l4_addr_t *)&hw_map_vid_mem_addr) < 0)
    return -L4_ENODEV;

  /* Linux driver comment: select non-DMA memory for PCI_MGA_DATA, otherwise
   * dump of PCI cfg space can lock PCI bus */
  PCIBIOS_WRITE_CONFIG_DWORD (bus, devfn, 0x44, 0x00003C00);

  if (b->flags & DEVF_G450DAC)
    /* Initialize Z_ORG register with zero, even if we do not use the 
     * Z buffer at all. We must do this always, BIOS does not do it for
     * us and accelerator dies without it */
    mga_outl(0x1C0C, 0);

  matrox_init(b->base->accelID);

  accel->copy = matrox_bmove;
  accel->fill = matrox_fill;
  accel->sync = matrox_sync;
  accel->pan  = matrox_pan;
//  accel->blit = matrox_blit;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  matrox_pci_bus_id   = bus;
  matrox_pci_devfn_id = devfn;

  /* has backend scaler which supports 420 mode */
  if (b->flags & DEVF_BES3PLANE)
    matrox_vid_init(accel);

  return 0;
}

void
matrox_register(void)
{
  pci_register(matrox_pci_tbl, matrox_probe);
}

