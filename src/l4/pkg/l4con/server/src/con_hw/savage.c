/*!
 * \file	savage.c
 * \brief	Hardware Acceleration for S3 Savage cards
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

#include <stdio.h>

#include <l4/sys/err.h>

#include "init.h"
#include "pci.h"
#include "savage.h"
#include "savage_regs.h"
#include "iomem.h"

#define SAVAGE_NEWMMIO_REGBASE_S3    0x1000000  /* 16MB */
#define SAVAGE_NEWMMIO_REGBASE_S4    0x0000000 
#define SAVAGE_NEWMMIO_REGSIZE	     0x0080000  /* 512kb */
#define SAVAGE_NEWMMIO_VGABASE	     0x8000

#define FIFO_CONTROL_REG	     0x8200
#define MIU_CONTROL_REG		     0x8204
#define STREAMS_TIMEOUT_REG	     0x8208
#define MISC_TIMEOUT_REG	     0x820c

#define MAXFIFO                      0x7f00

#define BCI_CMD_NOP                  0x40000000
#define BCI_CMD_SETREG               0x96000000
#define BCI_CMD_RECT                 0x48000000
#define BCI_CMD_RECT_XP              0x01000000
#define BCI_CMD_RECT_YP              0x02000000
#define BCI_CMD_SEND_COLOR           0x00008000
#define BCI_CMD_DEST_GBD             0x00000000
#define BCI_CMD_SRC_GBD              0x00000020
#define BCI_CMD_SRC_SOLID            0x00000000

#define BCI_BD_BW_DISABLE            0x10000000
#define BCI_BD_TILE_MASK             0x03000000
#define BCI_BD_TILE_NONE             0x00000000
#define BCI_BD_TILE_16               0x02000000
#define BCI_BD_TILE_32               0x04000000
#define BCI_BD_GET_BPP(bd)           (((bd) >> 16) & 0xFF)
#define BCI_BD_SET_BPP(bd, bpp)      ((bd) |= (((bpp) & 0xFF) << 16))
#define BCI_BD_GET_STRIDE(bd)        ((bd) & 0xFFFF)
#define BCI_BD_SET_STRIDE(bd, st)    ((bd) |= ((st) & 0xFFFF))

#define BCI_W_H(w, h)                (((h) << 16) | ((w) & 0xFFF))
#define BCI_X_Y(x, y)                (((y) << 16) | ((x) & 0xFFF))

#define BCI_GBD1                     0xE0
#define BCI_GBD2                     0xE1

#define BCI_BUFFER_OFFSET            0x10000
#define BCI_SIZE                     0x4000

#define BCI_GET_PTR		volatile unsigned int * bci_ptr = \
				  (unsigned int *) SavageBciMem
#define BCI_SEND(dw)		(*bci_ptr++ = (unsigned int)(dw))

#define PCI_CHIP_SAVAGE4		0x8a22
#define PCI_CHIP_SAVAGE3D		0x8a20
#define PCI_CHIP_SAVAGE3D_MV		0x8a21
#define PCI_CHIP_SAVAGE2000		0x9102
#define PCI_CHIP_SAVAGE_MX_MV		0x8c10
#define PCI_CHIP_SAVAGE_MX		0x8c11
#define PCI_CHIP_SAVAGE_IX_MV		0x8c12
#define PCI_CHIP_SAVAGE_IX		0x8c13
#define PCI_CHIP_PROSAVAGE_PM		0x8a25
#define PCI_CHIP_PROSAVAGE_KM		0x8a26
#define PCI_CHIP_SUPSAV_MX128		0x8c22
#define PCI_CHIP_SUPSAV_MX64		0x8c24
#define PCI_CHIP_SUPSAV_MX64C		0x8c26
#define PCI_CHIP_SUPSAV_IX128SDR	0x8c2a
#define PCI_CHIP_SUPSAV_IX128DDR	0x8c2b
#define PCI_CHIP_SUPSAV_IX64SDR		0x8c2c
#define PCI_CHIP_SUPSAV_IX64DDR		0x8c2d
#define PCI_CHIP_SUPSAV_IXCSDR		0x8c2e
#define PCI_CHIP_SUPSAV_IXCDDR		0x8c2f
#define PCI_CHIP_S3TWISTER_P		0x8d01
#define PCI_CHIP_S3TWISTER_K		0x8d02

enum savage_chips
{
  CH_SAVAGE4 = 0,
  CH_SAVAGE3D,
  CH_SAVAGE3D_MV,
  CH_SAVAGE2000,
  CH_SAVAGE_MX_MV,
  CH_SAVAGE_MX,
  CH_SAVAGE_IX_MV,
  CH_SAVAGE_IX,
  CH_PROSAVAGE_PM133,
  CH_PROSAVAGE_KM133,
  CH_TWISTER,
  CH_TWISTER_K,
  CH_SUPERSAVAGE_128,
  CH_SUPERSAVAGE_MX64,
  CH_SUPERSAVAGE_MX64C,
  CH_SUPERSAVAGE_IX128SDR,
  CH_SUPERSAVAGE_IX128DDR,
  CH_SUPERSAVAGE_IX64SDR,
  CH_SUPERSAVAGE_IX64DDR,
  CH_SUPERSAVAGE_IXCSDR,
  CH_SUPERSAVAGE_IXCDDR
};

struct board_t
{
  const char *name;
  savage_chipset chip;
};

static struct board_t savage_boards[] =
{
    { "Savage4", S3_SAVAGE4 },
    { "Savage3D", S3_SAVAGE3D },
    { "Savage3D-MV", S3_SAVAGE3D },
    { "Savage2000", S3_SAVAGE2000 },
    { "Savage/MX-MV", S3_SAVAGE_MX },
    { "Savage/MX", S3_SAVAGE_MX },
    { "Savage/IX-MV", S3_SAVAGE_MX },
    { "Savage/IX", S3_SAVAGE_MX },
    { "ProSavage PM133", S3_PROSAVAGE },
    { "ProSavage KM133", S3_PROSAVAGE },
    { "Twister", S3_PROSAVAGE },
    { "TwisterK", S3_PROSAVAGE },
    { "SuperSavage/MX 128", S3_SUPERSAVAGE },
    { "SuperSavage/MX 64", S3_SUPERSAVAGE },
    { "SuperSavage/MX 64C", S3_SUPERSAVAGE },
    { "SuperSavage/IX 128", S3_SUPERSAVAGE },
    { "SuperSavage/IX 128", S3_SUPERSAVAGE },
    { "SuperSavage/IX 64", S3_SUPERSAVAGE },
    { "SuperSavage/IX 64", S3_SUPERSAVAGE },
    { "SuperSavage/IXC 64", S3_SUPERSAVAGE },
    { "SuperSavage/IXC 64", S3_SUPERSAVAGE }
};

static const struct pci_device_id savage_pci_tbl[] __init =
{
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE4, 0, 0, CH_SAVAGE4},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE3D, 0, 0, CH_SAVAGE3D},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE3D_MV, 0, 0, CH_SAVAGE3D_MV},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE2000, 0, 0, CH_SAVAGE2000},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE_MX_MV, 0, 0, CH_SAVAGE_MX_MV},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE_MX, 0, 0, CH_SAVAGE_MX},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE_IX_MV, 0, 0, CH_SAVAGE_IX_MV},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SAVAGE_IX, 0, 0, CH_SAVAGE_IX},
    {PCI_VENDOR_ID_S3, PCI_CHIP_PROSAVAGE_PM, 0, 0, CH_PROSAVAGE_PM133},
    {PCI_VENDOR_ID_S3, PCI_CHIP_PROSAVAGE_KM, 0, 0, CH_PROSAVAGE_KM133},
    {PCI_VENDOR_ID_S3, PCI_CHIP_S3TWISTER_P, 0, 0, CH_TWISTER},
    {PCI_VENDOR_ID_S3, PCI_CHIP_S3TWISTER_K, 0, 0, CH_TWISTER_K},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_MX128, 0, 0, CH_SUPERSAVAGE_128},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_MX64, 0, 0, CH_SUPERSAVAGE_MX64},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_MX64C, 0, 0, CH_SUPERSAVAGE_MX64C},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IX128SDR, 0, 0, CH_SUPERSAVAGE_IX128SDR},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IX128DDR, 0, 0, CH_SUPERSAVAGE_IX128DDR},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IX64SDR, 0, 0, CH_SUPERSAVAGE_IX64SDR},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IX64DDR, 0, 0, CH_SUPERSAVAGE_IX64DDR},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IXCSDR, 0, 0, CH_SUPERSAVAGE_IXCSDR},
    {PCI_VENDOR_ID_S3, PCI_CHIP_SUPSAV_IXCDDR, 0, 0, CH_SUPERSAVAGE_IXCDDR},
    {0, 0, 0, 0, 0}
};

l4_addr_t savage_mmio;
unsigned int savage_chip;
void (*SavageWaitIdle)(void);
void (*SavageWaitFifo)(unsigned space);

static unsigned char* SavageBciMem;
static int ShadowStatus = 0;
static l4_addr_t ShadowPhysical;

/* Wait for fifo space */
static void
savage3D_waitfifo(unsigned space)
{
  unsigned slots = MAXFIFO - space;
  
  while ((savage_in32(0x48C00) & 0x0000ffff) > slots)
    ;
}

static void
savage4_waitfifo(unsigned space)
{
  unsigned slots = MAXFIFO - space;
  
  while ((savage_in32(0x48C60) & 0x001fffff) > slots)
    ;
}

static void
savage2000_waitfifo(unsigned space)
{
  unsigned slots = MAXFIFO - space;
  
  while ((savage_in32(0x48C60) & 0x0000ffff) > slots)
    ;
}

/* Wait for idle accelerator */
static void
savage3D_waitidle(void)
{
  while ((savage_in32(0x48C00) & 0x0008ffff) != 0x80000)
    ;
}

static void
savage4_waitidle(void)
{
 while ((savage_in32(0x48C60) & 0x00a00000) != 0x00a00000)
    ;
}

static void
savage2000_waitidle(void)
{
  while ((savage_in32(0x48C60) & 0x009fffff))
    ;
}

static void
savage_bmove (struct l4con_vc *vc,
	      int sx, int sy, int width, int height, int dx, int dy)
{
  (void)vc;
  BCI_GET_PTR;
  l4_uint32_t cmd = ( BCI_CMD_RECT |
		   BCI_CMD_DEST_GBD | BCI_CMD_SRC_GBD | (0xcc << 16) );

 if (dx < sx)
   cmd |= BCI_CMD_RECT_XP;
 else
   {
     sx += width-1;
     dx += width-1;
   }

 if (dy < sy)
   cmd |= BCI_CMD_RECT_YP;
 else
   {
     sy += height-1;
     dy += height-1;
   }

  SavageWaitFifo (4);

  BCI_SEND( cmd );
  BCI_SEND( BCI_X_Y( sx, sy ) );
  BCI_SEND( BCI_X_Y( dx, dy ) );
  BCI_SEND( BCI_W_H( width, height ) );
}

static void
savage_fill (struct l4con_vc *vc,
	    int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  BCI_GET_PTR;

  BCI_SEND( BCI_CMD_RECT | BCI_CMD_SEND_COLOR |
            BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
            BCI_CMD_DEST_GBD | BCI_CMD_SRC_SOLID | (0xcc << 16) );
  BCI_SEND( color );
  BCI_SEND( BCI_X_Y(sx, sy) );
  BCI_SEND( BCI_W_H(width, height) );
}

static void
savage_pan(int *x, int *y)
{
  int base = ((*y* hw_xres + *x) * (hw_bits/8)) >> 2;

  vga_out16(0x3d4, ((base & 0x0000ff) << 8) | 0x0d);
  vga_out16(0x3d4,  (base & 0x00ff00)       | 0x0c);
  vga_out16(0x3d4, ((base & 0x7f0000) >> 8) | 0x69);
}

static void
savage_sync(void)
{
  SavageWaitIdle();
}

static void
savage_setGBD(void)
{
  unsigned long GlobalBitmapDescriptor;

  GlobalBitmapDescriptor = 1 | 8 | BCI_BD_BW_DISABLE;
  BCI_BD_SET_BPP(GlobalBitmapDescriptor, hw_bits);
  BCI_BD_SET_STRIDE(GlobalBitmapDescriptor, hw_xres);

  /* Turn on 16-bit register access. */
  vga_out8(0x3d4, 0x31);
  vga_out8(0x3d5, 0x0c);

  /* Set stride to use GBD. */
  vga_out8(0x3d4, 0x50);
  vga_out8(0x3d5, vga_in8(0x3d5) | 0xC1);

  /* Enable 2D engine. */
  vga_out16(0x3d4, 0x0140);

  SavageWaitIdle();

  /* Now set the GBD and SBDs. */
  savage_out32(0x8168, 0);
  savage_out32(0x816C, GlobalBitmapDescriptor);
  savage_out32(0x8170, 0);
  savage_out32(0x8174, GlobalBitmapDescriptor);
  savage_out32(0x8178, 0);
  savage_out32(0x817C, GlobalBitmapDescriptor);

  savage_out32(0x81c8, hw_xres * hw_bits >> 3);
  savage_out32(0x81d8, hw_xres * hw_bits >> 3);
}

static void
savage_initialize2DEngine(void)
{
  unsigned long cobOffset;
  unsigned long cobSize;
  unsigned long cobIndex;

  if (S3_SAVAGE4_SERIES(savage_chip))
    {
      cobIndex = 2;
      cobSize = 0x8000 << cobIndex;
      cobOffset = hw_vid_mem_size;
    }
  else
    {
      cobSize = 1 << 17;
      if (savage_chip == S3_SUPERSAVAGE)
	cobIndex = 2;
      else
	cobIndex = 7;
      cobOffset = hw_vid_mem_size - cobSize;
    }

  vga_out16(0x3d4, 0x0140);
  vga_out8(0x3d4, 0x31);
  vga_out8(0x3d5, 0x0c);

  /* Setup plane masks */
  savage_out32(0x8128, ~0); /* enable all write planes */
  savage_out32(0x812C, ~0); /* enable all read planes */
  savage_out16(0x8134, 0x27);
  savage_out16(0x8136, 0x07);

  switch(savage_chip)
    {
    case S3_SAVAGE3D:
    case S3_SAVAGE_MX:
      /* Disable BCI */
      savage_out32(0x48C18, savage_in32(0x48C18) & 0x3FF0);
      /* Setup BCI command overflow buffer */
      savage_out32(0x48C14, (cobOffset >> 11) | (cobIndex << 29));
      /* Program shadow status update. */
      savage_out32(0x48C10, 0x78207220);
      if (ShadowStatus)
	{
	  savage_out32(0x48C0C, ShadowPhysical | 1);
	  /* Enable BCI and command overflow buffer */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x0E);
	}
      else
	{
	  savage_out32(0x48C0C, 0);
	  /* Enable BCI and command overflow buffer */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x0C);
	}
      break;

    case S3_SAVAGE4:
    case S3_PROSAVAGE:
    case S3_SUPERSAVAGE:
      /* Disable BCI */
      savage_out32(0x48C18, savage_in32(0x48C18) & 0x3FF0);
      /* Program shadow status update */
      savage_out32(0x48C10, 0x00700040);
      if (ShadowStatus)
	{
	  savage_out32(0x48C0C, ShadowPhysical | 1 );
	  /* Enable BCI without the COB */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x0a);
	}
      else
	{
	  savage_out32(0x48C0C, 0);
	  /* Enable BCI without the COB */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x08);
	}
      break;

    case S3_SAVAGE2000:
      /* Disable BCI */
      savage_out32(0x48C18, 0);
      /* Setup BCI command overflow buffer */
      savage_out32(0x48C18, (cobOffset >> 7) | (cobIndex));
      if (ShadowStatus)
	{
	  /* Set shadow update threshholds. */
	  savage_out32(0x48C10, 0x6090 );
	  savage_out32(0x48C14, 0x70A8 );
	  /* Enable shadow status update */
	  savage_out32(0x48A30, ShadowPhysical);
	  /* Enable BCI, command overflow buffer and shadow status. */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x00380000 );
	}
      else
	{
	  /* Disable shadow status update */
	  savage_out32(0x48A30, 0);
	  /* Enable BCI and command overflow buffer */
	  savage_out32(0x48C18, savage_in32(0x48C18) | 0x00280000 );
	}
      break;
    default:
      printf("not handled?\n");
      break;
    }

    /* Use and set global bitmap descriptor. */

    /* For reasons I do not fully understand yet, on the Savage4, the */
    /* write to the GBD register, MM816C, does not "take" at this time. */
    /* Only the low-order byte is acknowledged, resulting in an incorrect */
    /* stride.  Writing the register later, after the mode switch, works */
    /* correctly.  This needs to get resolved. */

  savage_setGBD();
  savage_setGBD();
} 


static int
savage_init(void)
{
  if (S3_SAVAGE3D_SERIES(savage_chip))
    {
      SavageWaitIdle = savage3D_waitidle;
      SavageWaitFifo = savage3D_waitfifo;
    }
  else if (S3_SAVAGE4_SERIES(savage_chip))
    {
      SavageWaitIdle = savage4_waitidle;
      SavageWaitFifo = savage4_waitfifo;
    }
  else
    {
      SavageWaitIdle = savage2000_waitidle;
      SavageWaitFifo = savage2000_waitfifo;
    }

  SavageBciMem = (unsigned char*)savage_mmio + BCI_BUFFER_OFFSET;

  savage_initialize2DEngine();

  return 0;
}

static int
savage_probe(unsigned int bus, unsigned int devfn,
             const struct pci_device_id *dev, con_accel_t *accel)
{
  unsigned int addr0, addr1;
  unsigned char rev;
  struct board_t *b = &savage_boards[dev->driver_data];

  PCIBIOS_READ_CONFIG_BYTE (bus, devfn, PCI_REVISION_ID, &rev);
  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_0, &addr0);

  printf("Found %s rev 0x%02x (PCI %02x/%02x)\n", b->name, rev, bus, devfn);

  if (map_io_mem(addr0, hw_vid_mem_size, 1, "video",
	         (l4_addr_t *)&hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  if (S3_SAVAGE3D_SERIES(b->chip))
    addr1 = addr0 + SAVAGE_NEWMMIO_REGBASE_S3;
  else
    addr1 = addr0 + SAVAGE_NEWMMIO_REGBASE_S4;

  if (map_io_mem(addr1, SAVAGE_NEWMMIO_REGSIZE, 0, "ctrl", &savage_mmio)<0)
    return -L4_ENODEV;

  savage_chip = b->chip;

  savage_init();

  accel->copy = savage_bmove;
  accel->fill = savage_fill;
  accel->sync = savage_sync;
  accel->pan  = savage_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  savage_vid_init(accel);

  return 0;
}

void
savage_register(void)
{
  pci_register(savage_pci_tbl, savage_probe);
}

