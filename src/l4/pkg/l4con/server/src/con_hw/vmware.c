/*!
 * \file	vmware.c
 * \brief	Use capabilities of VMware driver
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
#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/util/port_io.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "vmware.h"

#define PCI_CHIP_VMWARE0405		0x0405
#define PCI_CHIP_VMWARE0710		0x0710

#define PCI_DEVICE_ID_VMWARE_SVGA2	0x0405
#define PCI_DEVICE_ID_VMWARE_SVGA	0x0710
#define PCI_VENDOR_ID_VMWARE		0x15AD

#define SVGA_LEGACY_BASE_PORT		0x4560
#define SVGA_INDEX_PORT			0x0
#define SVGA_VALUE_PORT			0x1
#define SVGA_BIOS_PORT			0x2
#define SVGA_NUM_PORTS			0x3

#define	 SVGA_FIFO_MIN	      		0
#define	 SVGA_FIFO_MAX	      		1
#define	 SVGA_FIFO_NEXT_CMD   		2
#define	 SVGA_FIFO_STOP	      		3

#define SVGA_MAGIC         0x900000U
#define SVGA_MAKE_ID(ver)  (SVGA_MAGIC << 8 | (ver))

/* Version 2 let the address of the frame buffer be unsigned on Win32 */
#define SVGA_VERSION_2     2
#define SVGA_ID_2          SVGA_MAKE_ID(SVGA_VERSION_2)

/* Version 1 has new registers starting with SVGA_REG_CAPABILITIES so
   PALETTE_BASE has moved */
#define SVGA_VERSION_1     1
#define SVGA_ID_1          SVGA_MAKE_ID(SVGA_VERSION_1)

/* Version 0 is the initial version */
#define SVGA_VERSION_0     0
#define SVGA_ID_0          SVGA_MAKE_ID(SVGA_VERSION_0)

/* Invalid SVGA_ID_ */
#define SVGA_ID_INVALID    0xFFFFFFFF

enum 
{
  SVGA_REG_ID = 0,
  SVGA_REG_ENABLE = 1,
  SVGA_REG_WIDTH = 2,
  SVGA_REG_HEIGHT = 3,
  SVGA_REG_MAX_WIDTH = 4,
  SVGA_REG_MAX_HEIGHT = 5,
  SVGA_REG_DEPTH = 6,
  SVGA_REG_BITS_PER_PIXEL = 7,
  SVGA_REG_PSEUDOCOLOR = 8,
  SVGA_REG_RED_MASK = 9,
  SVGA_REG_GREEN_MASK = 10,
  SVGA_REG_BLUE_MASK = 11,
  SVGA_REG_BYTES_PER_LINE = 12,
  SVGA_REG_FB_START = 13,
  SVGA_REG_FB_OFFSET = 14,
  SVGA_REG_FB_MAX_SIZE = 15,
  SVGA_REG_FB_SIZE = 16,
  
  SVGA_REG_CAPABILITIES = 17,
  SVGA_REG_MEM_START = 18,	   /* Memory for command FIFO and bitmaps */
  SVGA_REG_MEM_SIZE = 19,
  SVGA_REG_CONFIG_DONE = 20,       /* Set when memory area configured */
  SVGA_REG_SYNC = 21,              /* Write to force synchronization */
  SVGA_REG_BUSY = 22,              /* Read to check if sync is done */
  SVGA_REG_GUEST_ID = 23,	   /* Set guest OS identifier */
  SVGA_REG_CURSOR_ID = 24,	   /* ID of cursor */
  SVGA_REG_CURSOR_X = 25,	   /* Set cursor X position */
  SVGA_REG_CURSOR_Y = 26,	   /* Set cursor Y position */
  SVGA_REG_CURSOR_ON = 27,	   /* Turn cursor on/off */
  
  SVGA_REG_TOP = 28,		   /* Must be 1 greater than the last reg */
  
  SVGA_PALETTE_BASE = 1024	   /* Base of SVGA color map */
};

/*
 *  Capabilities
 */

#define	SVGA_CAP_RECT_FILL	 0x0001
#define	SVGA_CAP_RECT_COPY	 0x0002
#define	SVGA_CAP_RECT_PAT_FILL   0x0004
#define	SVGA_CAP_OFFSCREEN       0x0008
#define	SVGA_CAP_RASTER_OP	 0x0010
#define	SVGA_CAP_CURSOR		 0x0020
#define	SVGA_CAP_CURSOR_BYPASS	 0x0040

/*
 *  Commands in the command FIFO
 */

#define	 SVGA_CMD_UPDATE		   1
	 /* FIFO layout:
	    X, Y, Width, Height */

#define	 SVGA_CMD_RECT_FILL		   2
	 /* FIFO layout:
	    Color, X, Y, Width, Height */

#define	 SVGA_CMD_RECT_COPY		   3
	 /* FIFO layout:
	    Source X, Source Y, Dest X, Dest Y, Width, Height */

#define	 SVGA_CMD_DEFINE_BITMAP		   4
	 /* FIFO layout:
	    Pixmap ID, Width, Height, <scanlines> */

#define	 SVGA_CMD_DEFINE_BITMAP_SCANLINE   5
	 /* FIFO layout:
	    Pixmap ID, Width, Height, Line #, scanline */

#define	 SVGA_CMD_DEFINE_PIXMAP		   6
	 /* FIFO layout:
	    Pixmap ID, Width, Height, Depth, <scanlines> */

#define	 SVGA_CMD_DEFINE_PIXMAP_SCANLINE   7
	 /* FIFO layout:
	    Pixmap ID, Width, Height, Depth, Line #, scanline */

#define	 SVGA_CMD_RECT_BITMAP_FILL	   8
	 /* FIFO layout:
	    Bitmap ID, X, Y, Width, Height, Foreground, Background */

#define	 SVGA_CMD_RECT_PIXMAP_FILL	   9
	 /* FIFO layout:
	    Pixmap ID, X, Y, Width, Height */

#define	 SVGA_CMD_RECT_BITMAP_COPY	  10
	 /* FIFO layout:
	    Bitmap ID, Source X, Source Y, Dest X, Dest Y,
	    Width, Height, Foreground, Background */

#define	 SVGA_CMD_RECT_PIXMAP_COPY	  11
	 /* FIFO layout:
	    Pixmap ID, Source X, Source Y, Dest X, Dest Y, Width, Height */

#define	 SVGA_CMD_FREE_OBJECT		  12
	 /* FIFO layout:
	    Object (pixmap, bitmap, ...) ID */

#define	 SVGA_CMD_RECT_ROP_FILL           13
         /* FIFO layout:
            Color, X, Y, Width, Height, ROP */

#define	 SVGA_CMD_RECT_ROP_COPY           14
         /* FIFO layout:
            Source X, Source Y, Dest X, Dest Y, Width, Height, ROP */

#define	 SVGA_CMD_RECT_ROP_BITMAP_FILL    15
         /* FIFO layout:
            ID, X, Y, Width, Height, Foreground, Background, ROP */

#define	 SVGA_CMD_RECT_ROP_PIXMAP_FILL    16
         /* FIFO layout:
            ID, X, Y, Width, Height, ROP */

#define	 SVGA_CMD_RECT_ROP_BITMAP_COPY    17
         /* FIFO layout:
            ID, Source X, Source Y,
            Dest X, Dest Y, Width, Height, Foreground, Background, ROP */

#define	 SVGA_CMD_RECT_ROP_PIXMAP_COPY    18
         /* FIFO layout:
            ID, Source X, Source Y, Dest X, Dest Y, Width, Height, ROP */

#define	SVGA_CMD_DEFINE_CURSOR		  19
	/* FIFO layout:
	   ID, Hotspot X, Hotspot Y, Width, Height,
	   Depth for AND mask, Depth for XOR mask,
	   <scanlines for AND mask>, <scanlines for XOR mask> */

#define	SVGA_CMD_DISPLAY_CURSOR		  20
	/* FIFO layout:
	   ID, On/Off (1 or 0) */

#define	SVGA_CMD_MOVE_CURSOR		  21
	/* FIFO layout:
	   X, Y */

#define	SVGA_CMD_MAX			  22

static const struct pci_device_id vmware_pci_tbl[] __init =
{
    {PCI_VENDOR_ID_VMWARE, PCI_CHIP_VMWARE0405, 0, 0, 0},
    {PCI_VENDOR_ID_VMWARE, PCI_CHIP_VMWARE0710, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

static unsigned vm_idx;
static unsigned vm_val;
static unsigned *vm_fifo;
static unsigned vm_caps;
static unsigned vm_may_be_busy = 0;


static unsigned
vmwareReadReg(int index)
{
  l4util_out32(index, vm_idx);
  return l4util_in32(vm_val);
}

static void
vmwareWriteReg(int index, unsigned value)
{
  l4util_out32(index, vm_idx);
  l4util_out32(value, vm_val);
}

static void
vmwareWriteWordToFIFO(unsigned value)
{
  /* Need to sync? */
  if ((   vm_fifo[SVGA_FIFO_NEXT_CMD]+sizeof(unsigned) 
	== vm_fifo[SVGA_FIFO_STOP])
      || (vm_fifo[SVGA_FIFO_NEXT_CMD] 
	== vm_fifo[SVGA_FIFO_MAX] - sizeof(unsigned) &&
	vm_fifo[SVGA_FIFO_STOP] == vm_fifo[SVGA_FIFO_MIN])) 
    {
      vmwareWriteReg(SVGA_REG_SYNC, 1);
      while (vmwareReadReg(SVGA_REG_BUSY))
	;
    }
  vm_fifo[vm_fifo[SVGA_FIFO_NEXT_CMD] / sizeof(unsigned)] = value;
  vm_fifo[SVGA_FIFO_NEXT_CMD] += sizeof(unsigned);
  if (vm_fifo[SVGA_FIFO_NEXT_CMD] == vm_fifo[SVGA_FIFO_MAX]) 
    {
      vm_fifo[SVGA_FIFO_NEXT_CMD] = vm_fifo[SVGA_FIFO_MIN];
    }
}

static void
vmwareWaitForFB(void)
{
  if (vm_may_be_busy)
    {
      vmwareWriteReg(SVGA_REG_SYNC, 1);
      while (vmwareReadReg(SVGA_REG_BUSY))
	;
      vm_may_be_busy = 0;
    }
}

static int
vmwareInitFIFO(void)
{
  unsigned mmioPhysBase = vmwareReadReg(SVGA_REG_MEM_START);
  unsigned mmioSize = vmwareReadReg(SVGA_REG_MEM_SIZE) & ~3;
  l4_addr_t mmioVirtBase;

  if (map_io_mem(mmioPhysBase, mmioSize, 0, "ctrl", &mmioVirtBase) < 0)
    return -L4_EINVAL;

  vm_fifo = (unsigned*)mmioVirtBase;

  vm_fifo[SVGA_FIFO_MIN]      = 4 * sizeof(unsigned);
  vm_fifo[SVGA_FIFO_MAX]      = mmioSize;
  vm_fifo[SVGA_FIFO_NEXT_CMD] = 4 * sizeof(unsigned);
  vm_fifo[SVGA_FIFO_STOP]     = 4 * sizeof(unsigned);
  vmwareWriteReg(SVGA_REG_CONFIG_DONE, 1);

  return 0;
}

static void
vmwareSendSVGACmdUpdate(int x, int y, int w, int h)
{
  vmwareWriteWordToFIFO(SVGA_CMD_UPDATE);
  vmwareWriteWordToFIFO(x);
  vmwareWriteWordToFIFO(y);
  vmwareWriteWordToFIFO(w);
  vmwareWriteWordToFIFO(h);
}

static void
vmwareRectFill(struct l4con_vc *vc,
	       int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  vmwareWriteWordToFIFO(SVGA_CMD_RECT_FILL);
  vmwareWriteWordToFIFO(color);
  vmwareWriteWordToFIFO(sx);
  vmwareWriteWordToFIFO(sy);
  vmwareWriteWordToFIFO(width);
  vmwareWriteWordToFIFO(height);
  
  vm_may_be_busy = 1;
}

static void
vmwareRectCopy(struct l4con_vc *vc, int sx, int sy,
	       int width, int height, int dx, int dy)
{
  (void)vc;
  vmwareWriteWordToFIFO(SVGA_CMD_RECT_COPY);
  vmwareWriteWordToFIFO(sx);
  vmwareWriteWordToFIFO(sy);
  vmwareWriteWordToFIFO(dx);
  vmwareWriteWordToFIFO(dy);
  vmwareWriteWordToFIFO(width);
  vmwareWriteWordToFIFO(height);
  
  vm_may_be_busy = 1;
}

static unsigned
VMXGetVMwareSvgaId(void)
{
   unsigned vmware_svga_id;

   vmwareWriteReg(SVGA_REG_ID, SVGA_ID_2);
   vmware_svga_id = vmwareReadReg(SVGA_REG_ID);
   if (vmware_svga_id == SVGA_ID_2) 
     return SVGA_ID_2;
   
   vmwareWriteReg(SVGA_REG_ID, SVGA_ID_1);
   vmware_svga_id = vmwareReadReg(SVGA_REG_ID);
   if (vmware_svga_id == SVGA_ID_1) 
     return SVGA_ID_1;
   
   if (vmware_svga_id == SVGA_ID_0) 
     return SVGA_ID_0;
   
   /* No supported VMware SVGA devices found */
   return SVGA_ID_INVALID;
}

static int
vmware_probe(unsigned int bus, unsigned int devfn, 
	     const struct pci_device_id *dev, con_accel_t *accel)
{
  l4_addr_t addr;
  l4_size_t size;
  unsigned id;

  printf("Found VMware %04x (PCI %02x/%02x)", dev->device, bus, devfn);

  if (dev->device == PCI_CHIP_VMWARE0710)
    {
      vm_idx = SVGA_LEGACY_BASE_PORT + SVGA_INDEX_PORT*sizeof(unsigned);
      vm_val = SVGA_LEGACY_BASE_PORT + SVGA_VALUE_PORT*sizeof(unsigned);
    }
  else
    {
      pci_resource(bus, devfn, 0, &addr, &size);
      vm_idx = addr + SVGA_INDEX_PORT;
      vm_val = addr + SVGA_VALUE_PORT;
    }

  id = VMXGetVMwareSvgaId();
  if (id == SVGA_ID_0 || id == SVGA_ID_INVALID)
    {
      printf("\nNot supported VMware SVGA found (read ID 0x%08x).\n", id);
      return -L4_ENODEV;
    }

  vm_caps = vmwareReadReg(SVGA_REG_CAPABILITIES);

  printf(" caps=");
  if (vm_caps & SVGA_CAP_RECT_FILL)
    {
      accel->fill = vmwareRectFill;
      accel->caps |= ACCEL_FAST_FILL;
      printf("fastfill");
    }
  if (vm_caps & SVGA_CAP_RECT_COPY)
    {
      accel->copy = vmwareRectCopy;
      accel->caps |= ACCEL_FAST_COPY;
      printf("%sfastcopy", accel->caps ? "," : "");
    }
  printf("%s\n", accel->caps ? "" : "none");

  if (vmwareInitFIFO()<0)
    return -L4_ENODEV;

  accel->sync = vmwareWaitForFB;
  accel->drty = vmwareSendSVGACmdUpdate;

  /* any drawing functions other that fast fill and fast copy have to
   * call dirty to notify VMware to redraw the screen. */
  accel->caps |= ACCEL_POST_DIRTY;

  hw_vid_mem_addr = vmwareReadReg(SVGA_REG_FB_START);
  hw_vid_mem_size = vmwareReadReg(SVGA_REG_FB_MAX_SIZE);

  if (map_io_mem(hw_vid_mem_addr, hw_vid_mem_size,
		1, "video", (l4_addr_t *)&hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  hw_map_vid_mem_addr += vmwareReadReg(SVGA_REG_FB_OFFSET);

  return 0;
}

void
vmware_register(void)
{
  pci_register(vmware_pci_tbl, vmware_probe);
}
