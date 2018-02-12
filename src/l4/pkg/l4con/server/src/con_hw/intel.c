/*!
 * \file	intel.c
 * \brief	Intel I830 driver
 *
 * \date	10/2004
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* Most stuff was taken from intelfb Linux driver of David Dawes. */

#include <stdio.h>
#include <l4/sys/types.h>
#include <l4/util/port_io.h>
#include <l4/util/rdtsc.h>
#include <l4/util/l4_macros.h>
#include <l4/sys/err.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "intel.h"

#define PCI_DEVICE_ID_INTEL_830M	0x3577
#define PCI_DEVICE_ID_INTEL_845G	0x2562
#define PCI_DEVICE_ID_INTEL_85XGM	0x3582
#define PCI_DEVICE_ID_INTEL_865G	0x2572
#define PCI_DEVICE_ID_INTEL_915G	0x2582
#define PCI_DEVICE_ID_INTEL_915GM	0x2592

#define INTEL_85X_CAPID		0x44
#define INTEL_85X_VARIANT_MASK		0x7
#define INTEL_85X_VARIANT_SHIFT		5
#define INTEL_VAR_855GME		0x0
#define INTEL_VAR_855GM			0x4
#define INTEL_VAR_852GME		0x2
#define INTEL_VAR_852GM			0x5
#define INTEL_GMCH_CTRL		0x52

#define INTEL_REG_SIZE			0x80000

static l4_addr_t   mmio_base;
static l4_addr_t   fb_base;
static l4_addr_t   fb_offset;
static l4_uint32_t pitch;
static int         blitter_may_be_busy;

static l4_addr_t   ring_base, ring_base_phys;
static l4_uint32_t ring_space;
static l4_uint32_t ring_head, ring_tail;
const  l4_uint32_t ring_size      = 64*1024;
const  l4_uint32_t ring_tail_mask = 64*1024 - 1;

static const struct pci_device_id intel_pci_tbl[] __init =
{
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_830M,  0, 0, 0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_845G,  0, 0, 0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_85XGM, 0, 0, 0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_865G,  0, 0, 0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_915G,  0, 0, 0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_915GM, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};

/* Memory Commands */
#define MI_NOOP			(0x00 << 23)
#define MI_NOOP_WRITE_ID		(1 << 22)
#define MI_NOOP_ID_MASK			((1 << 22) - 1)

#define MI_FLUSH		(0x04 << 23)
#define MI_WRITE_DIRTY_STATE		(1 << 4)
#define MI_END_SCENE			(1 << 3)
#define MI_INHIBIT_RENDER_CACHE_FLUSH	(1 << 2)
#define MI_INVALIDATE_MAP_CACHE		(1 << 0)

#define MI_STORE_DWORD_IMM	((0x20 << 23) | 1)

/* 2D Commands */
#define COLOR_BLT_CMD		((2 << 29) | (0x40 << 22) | 3)
#define XY_COLOR_BLT_CMD	((2 << 29) | (0x50 << 22) | 4)
#define XY_SETUP_CLIP_BLT_CMD	((2 << 29) | (0x03 << 22) | 1)
#define XY_SRC_COPY_BLT_CMD	((2 << 29) | (0x53 << 22) | 6)
#define SRC_COPY_BLT_CMD	((2 << 29) | (0x43 << 22) | 4)
#define XY_MONO_PAT_BLT_CMD	((2 << 29) | (0x52 << 22) | 7)
#define XY_MONO_SRC_BLT_CMD	((2 << 29) | (0x54 << 22) | 6)
#define XY_MONO_SRC_IMM_BLT_CMD	((2 << 29) | (0x71 << 22) | 5)

#define WRITE_ALPHA			(1 << 21)
#define WRITE_RGB			(1 << 20)
#define VERT_SEED			(3 << 8)
#define HORIZ_SEED			(3 << 12)

#define COLOR_DEPTH_8			(0 << 24)
#define COLOR_DEPTH_16			(1 << 24)
#define COLOR_DEPTH_32			(3 << 24)

#define SRC_ROP_GXCOPY			0xcc
#define SRC_ROP_GXXOR			0x66

#define PAT_ROP_GXCOPY                  0xf0
#define PAT_ROP_GXXOR                   0x5a

#define PITCH_SHIFT			0
#define ROP_SHIFT			16
#define WIDTH_SHIFT			0
#define HEIGHT_SHIFT			16

/* Primary ring buffer */
#define PRI_RING_TAIL		0x2030
#define RING_TAIL_MASK			0x001ffff8
#define RING_INUSE			0x1
#define PRI_RING_HEAD		0x2034
#define RING_HEAD_WRAP_MASK		0x7ff
#define RING_HEAD_WRAP_SHIFT		21
#define RING_HEAD_MASK			0x001ffffc
#define PRI_RING_START		0x2038
#define RING_START_MASK			0xfffff000
#define PRI_RING_LENGTH		0x203c
#define RING_LENGTH_MASK		0x001ff000
#define RING_REPORT_MASK		(0x3 << 1)
#define RING_NO_REPORT			(0x0 << 1)
#define RING_REPORT_64K			(0x1 << 1)
#define RING_REPORT_4K			(0x2 << 1)
#define RING_REPORT_128K		(0x3 << 1)
#define RING_ENABLE			0x1

#define RING_MIN_FREE		64
#define GTT_PAGE_SIZE		4096

#define DSPABASE		0x70184

/* Ring buffer macros */
#define OUT_RING(n)	do {						\
	*(volatile l4_uint32_t*)(ring_base + ring_tail) = n;		\
	ring_tail += 4;							\
	ring_tail &= ring_tail_mask;					\
} while (0)

#define START_RING(n)	do {						\
	l4_uint32_t needed = (n) * 4;					\
	if (ring_tail + needed > ring_tail_mask - RING_MIN_FREE) {	\
		needed += ring_tail_mask + 1 - ring_tail;		\
	}								\
	if (ring_space < needed)					\
		wait_ring(needed);					\
	ring_space -= needed;						\
	while (needed > (n) * 4) {					\
		OUT_RING(MI_NOOP);					\
		needed -= 4;						\
	}								\
} while (0)

#define ADVANCE_RING()	do {						\
	outreg32(PRI_RING_TAIL, ring_tail);				\
} while (0)

static inline l4_uint32_t
inreg32(l4_uint32_t addr)
{
  return *(volatile l4_uint32_t*)(mmio_base + addr);
}

static inline void
outreg32(l4_uint32_t addr, l4_uint32_t val)
{
  *(volatile l4_uint32_t*)(mmio_base + addr) = val;
}

static void
wait_ring(unsigned n)
{
#define ms_300	300000000ULL
  l4_uint64_t end = l4_rdtsc() + l4_ns_to_tsc(ms_300);
  l4_uint32_t last_head = inreg32(PRI_RING_HEAD) & RING_HEAD_MASK;

  while (ring_space < n)
    {
      ring_head  = inreg32(PRI_RING_HEAD) & RING_HEAD_MASK;
      if (ring_tail + RING_MIN_FREE < ring_head)
	ring_space = ring_head - ring_tail - RING_MIN_FREE;
      else
	ring_space = ring_size + ring_head - ring_tail - RING_MIN_FREE;
      if (ring_head != last_head)
	{
	  end = l4_rdtsc() + l4_ns_to_tsc(ms_300);
	  last_head = ring_head;
	}
      if (end < l4_rdtsc())
	{
	  printf("space: %d wanted %d -- lookup\n", ring_space, n);
	  break;
	}
      l4_busy_wait_ns(1000);
    }
}

static void
refresh_ring(void)
{
  ring_head  = inreg32(PRI_RING_HEAD) & RING_HEAD_MASK;
  ring_tail  = inreg32(PRI_RING_TAIL) & RING_TAIL_MASK;
  if (ring_tail + RING_MIN_FREE < ring_head)
    ring_space = ring_head - ring_tail - RING_MIN_FREE;
  else
    ring_space = ring_size + ring_head - ring_tail - RING_MIN_FREE;
}

static void
intel_sync(void)
{
  if (!blitter_may_be_busy)
    return;

  START_RING(2);
  OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
  OUT_RING(MI_NOOP);
  ADVANCE_RING();
  wait_ring(ring_size - RING_MIN_FREE);
  ring_space = ring_size - RING_MIN_FREE;
  blitter_may_be_busy = 0;
}

static void
intel_pan(int *x, int *y)
{
  l4_uint32_t offset, xoffset, yoffset;

  *x &= 7;
  xoffset = *x;
  yoffset = *y;
  offset  = yoffset * pitch + (xoffset * hw_bits) / 8;
  offset += fb_offset << 12;

  outreg32(DSPABASE, offset);
}

static void 
intel_bmove(struct l4con_vc *vc,
	    int sx, int sy, int width, int height, int dx, int dy)
{
  (void)vc;
  l4_uint32_t br00, br09, br11, br12, br13, br22, br23, br26;

  br00 = XY_SRC_COPY_BLT_CMD;
  br09 = fb_offset;
  br11 = (pitch << PITCH_SHIFT);
  br12 = fb_offset;
  br13 = (SRC_ROP_GXCOPY << ROP_SHIFT) | (pitch << PITCH_SHIFT);
  br22 = (dx << WIDTH_SHIFT) | (dy << HEIGHT_SHIFT);
  br23 = ((dx + width) << WIDTH_SHIFT) | ((dy + height) << HEIGHT_SHIFT);
  br26 = (sx << WIDTH_SHIFT) | (sy << HEIGHT_SHIFT);

  switch (hw_bits)
    {
    case  8: br13 |= COLOR_DEPTH_8;                                   break;
    case 16: br13 |= COLOR_DEPTH_16;                                  break;
    case 32: br13 |= COLOR_DEPTH_32; br00 |= WRITE_ALPHA | WRITE_RGB; break;
    }

  START_RING(8);
  OUT_RING(br00);
  OUT_RING(br13);
  OUT_RING(br22);
  OUT_RING(br23);
  OUT_RING(br09);
  OUT_RING(br26);
  OUT_RING(br11);
  OUT_RING(br12);
  ADVANCE_RING();

  blitter_may_be_busy = 1;
}

static void 
intel_fill(struct l4con_vc *vc,
	   int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  l4_uint32_t br00, br09, br13, br14, br16;

  br00 = COLOR_BLT_CMD;
  br09 = fb_offset + (sy * pitch + sx * (hw_bits / 8));
  br13 = (PAT_ROP_GXCOPY << ROP_SHIFT) | pitch;
  br14 = (height << HEIGHT_SHIFT) | ((width * (hw_bits / 8)) << WIDTH_SHIFT);
  br16 = color;

  switch (hw_bits)
    {
    case 8:  br13 |= COLOR_DEPTH_8;		                      break;
    case 16: br13 |= COLOR_DEPTH_16;		                      break;
    case 32: br13 |= COLOR_DEPTH_32; br00 |= WRITE_ALPHA | WRITE_RGB; break;
    }

  START_RING(6);
  OUT_RING(br00);
  OUT_RING(br13);
  OUT_RING(br14);
  OUT_RING(br09);
  OUT_RING(br16);
  OUT_RING(MI_NOOP);
  ADVANCE_RING();

  blitter_may_be_busy = 1;
}

static void
intel_2d_start(void)
{
  outreg32(PRI_RING_LENGTH, 0);
  outreg32(PRI_RING_TAIL, 0);
  outreg32(PRI_RING_HEAD, 0);

  outreg32(PRI_RING_START, ring_base_phys & RING_START_MASK);
  outreg32(PRI_RING_LENGTH, ((ring_size - GTT_PAGE_SIZE) & RING_LENGTH_MASK)
			     | RING_NO_REPORT | RING_ENABLE);
  refresh_ring();
}

static int
intel_probe(unsigned int bus, unsigned int devfn,
	    const struct pci_device_id *dev, con_accel_t *accel)
{
  unsigned tmp;
  l4_addr_t addr;
  l4_size_t size;
  const char *name;
  int aperture = 0, mmio = 1;

  switch (dev->device)
    {
    case PCI_DEVICE_ID_INTEL_830M:  name = "830M"; break;
    case PCI_DEVICE_ID_INTEL_845G:  name = "845G"; break;
    case PCI_DEVICE_ID_INTEL_865G:  name = "865G"; break;
    case PCI_DEVICE_ID_INTEL_85XGM:
    case PCI_DEVICE_ID_INTEL_915G:  name = "915G"; break;
    case PCI_DEVICE_ID_INTEL_915GM: name = "915GM"; break;
    default:
      PCIBIOS_READ_CONFIG_DWORD(bus, devfn, INTEL_85X_CAPID, &tmp);
      switch ((tmp >> INTEL_85X_VARIANT_SHIFT)& INTEL_85X_VARIANT_MASK)
	{
	case INTEL_VAR_855GME: name = "855GME"; break;
	case INTEL_VAR_855GM:  name = "855GM";  break;
	case INTEL_VAR_852GME: name = "852GME"; break;
	case INTEL_VAR_852GM:  name = "852GM";  break;
	default:               name = "852GM/855GM"; break;
	}
      break;
    }

  if (dev->device == PCI_DEVICE_ID_INTEL_915G ||
      dev->device == PCI_DEVICE_ID_INTEL_915GM)
    {
      aperture = 2;
      mmio     = 0;
    }

  pci_resource(bus, devfn, aperture, &addr, &size);
  if (!addr)
    return -L4_ENODEV;

  if (size < hw_vid_mem_size)
    {
      printf("Aperture size = %zdKB (< %ldKB)\n", 
	    size/1024, hw_vid_mem_size/1024);
      return -L4_ENODEV;
    }

  if (size > hw_vid_mem_size)
    size = hw_vid_mem_size;

  if (map_io_mem(addr, size, 1, "video", &fb_base) < 0)
    return -L4_ENODEV;

  hw_map_vid_mem_addr = fb_base;

  ring_base_phys = addr    + size - (128<<10);
  ring_base      = fb_base + size - (128<<10);

  pci_resource(bus, devfn, mmio, &addr, &size);
  if (!addr)
    return -L4_ENODEV;

  if (map_io_mem(addr, INTEL_REG_SIZE, 0, "ctrl", &mmio_base) < 0)
    return -L4_ENODEV;


  printf("Found Intel (R) %s adapter at "l4_addr_fmt"\n", name, addr);

#if 0
  switch (hw_bits)
    {
    case 8:  pitch = hw_xres;     break;
    case 16: pitch = hw_xres * 2; break;
    case 32: pitch = hw_xres * 4; break;
    }
#endif
  pitch = hw_bpl;

  intel_2d_start();

  blitter_may_be_busy = 1;
  intel_sync();

  accel->copy = intel_bmove;
  accel->fill = intel_fill;
  accel->sync = intel_sync;
  accel->pan  = intel_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  return 0;
}

void
intel_register(void)
{
  pci_register(intel_pci_tbl, intel_probe);
}
