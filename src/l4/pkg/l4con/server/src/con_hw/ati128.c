/*!
 * \file	ati128.c
 * \brief	Hardware Acceleration for ATI Rage128 cards
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

/* most stuff taken from Linux 2.2.25: driver/video/aty128fb.c */

#include <stdio.h>

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/sys/consts.h>
#include <l4/util/util.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "ati128.h"
#include "aty128.h"

enum { rage_128=0x40, rage_128_pro=0x41, rage_M3=0x42 };


/* some device IDs not known in the old pci-not-24 library */
/* Rage128 Pro GL */
#define PCI_DEVICE_ID_ATI_RAGE128_PF	0x5046
/* Rage128 Pro VR */
#define PCI_DEVICE_ID_ATI_RAGE128_PR	0x5052
/* Rage128 GL */
#define PCI_DEVICE_ID_ATI_RAGE128_RE	0x5245
#define PCI_DEVICE_ID_ATI_RAGE128_RF	0x5246
/* Rage128 VR */
#define PCI_DEVICE_ID_ATI_RAGE128_RK	0x524b
#define PCI_DEVICE_ID_ATI_RAGE128_RL	0x524c
/* Rage128 M3 */
#define PCI_DEVICE_ID_ATI_RAGE128_LE	0x4c45
#define PCI_DEVICE_ID_ATI_RAGE128_LF	0x4c46

static const char * const ati128_ids[] =
{
  "Rage128 RE (PCI)", "Rage128 RF (AGP)", "Rage128 RK (PCI)",
  "Rage128 RL (AGP)", "Rage128 Pro PF (AGP)", "Rage128 Pro PR (PCI)",
  "Rage Mobility M3 (PCI)", "Rage Mobility M3 (AGP)" };

static const struct pci_device_id ati128_pci_tbl[] __init =
{
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_RE, 0, 0, 0 | rage_128},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_RF, 0, 0, 1 | rage_128},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_RK, 0, 0, 2 | rage_128},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_RL, 0, 0, 3 | rage_128},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_PF, 0, 0, 4 | rage_128_pro},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_PR, 0, 0, 5 | rage_128_pro},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_LE, 0, 0, 6 | rage_M3},
    {PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RAGE128_LF, 0, 0, 7 | rage_M3},
    {0, 0, 0, 0, 0}
 };

l4_addr_t ati128_regbase = 0;
int       ati128_supports_planar = 0;

static unsigned ati_pci_bus = 0;
static unsigned ati_pci_devfn = 0;


static int fifo_slots;
static int blitter_may_be_busy;


static unsigned
aty_ld_pll(unsigned pll_index)
{       
  aty_st_8(CLOCK_CNTL_INDEX, pll_index & 0x1F);
  return aty_ld_le32(CLOCK_CNTL_DATA);
}

static void
aty_st_pll(unsigned pll_index, unsigned val)
{   
  aty_st_8(CLOCK_CNTL_INDEX, (pll_index & 0x1F) | PLL_WR_EN);
  aty_st_le32(CLOCK_CNTL_DATA, val);
}

static void
aty128_flush_pixel_cache(void)
{
  int i;
  unsigned tmp;

  tmp = aty_ld_le32(PC_NGUI_CTLSTAT);
  tmp &= ~(0x00ff);
  tmp |= 0x00ff;
  aty_st_le32(PC_NGUI_CTLSTAT, tmp);

  for (i = 0; i < 2000000; i++)
    if (!(aty_ld_le32(PC_NGUI_CTLSTAT) & PC_BUSY))
      break;
}

static void
aty128_reset_engine(void)
{
  unsigned gen_reset_cntl, clock_cntl_index, mclk_cntl;

  aty128_flush_pixel_cache();

  clock_cntl_index = aty_ld_le32(CLOCK_CNTL_INDEX);
  mclk_cntl = aty_ld_pll(MCLK_CNTL);

  aty_st_pll(MCLK_CNTL, mclk_cntl | 0x00030000);

  gen_reset_cntl = aty_ld_le32(GEN_RESET_CNTL);
  aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl | SOFT_RESET_GUI);
  aty_ld_le32(GEN_RESET_CNTL);
  aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl & ~(SOFT_RESET_GUI));
  aty_ld_le32(GEN_RESET_CNTL);

  aty_st_pll(MCLK_CNTL, mclk_cntl);
  aty_st_le32(CLOCK_CNTL_INDEX, clock_cntl_index);
  aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl);

  /* use old pio mode */
  aty_st_le32(PM4_BUFFER_CNTL, PM4_BUFFER_CNTL_NONPM4);
}

static void
aty128_do_wait_for_fifo(unsigned short entries)
{
  int i;

  for (;;) 
    {
      for (i = 0; i < 2000000; i++) 
	{
     	  fifo_slots = aty_ld_le32(GUI_STAT) & 0x0fff;
   	  if (fifo_slots >= entries)
	    return;
	}
      aty128_reset_engine();
    }
}

void
aty128_wait_for_fifo(unsigned short entries)
{
  if (fifo_slots < entries)
    aty128_do_wait_for_fifo(64);
  fifo_slots -= entries;
}


void
aty128_wait_for_idle(void)
{
  int i;

  aty128_do_wait_for_fifo(64);

  for (;;) 
    {
      for (i=0; i<2000000; i++)
	{
	  if (!(aty_ld_le32(GUI_STAT) & (1 << 31)))
	    {
	      aty128_flush_pixel_cache();
	      blitter_may_be_busy = 0;
	      return;
	    }
	}
      aty128_reset_engine();
    }
}


#if 0

/* While we don't have I/O flexpages in Fiasco, we can't prohibit other
 * L4 tasks to write to I/O ports. In that case it is possible, that
 * we start X in L4Linux which does some things with the PCI configuration
 * area of our graphics card so that it's memory mapped registers are
 * no longer mapped. Test this case here and get back registers if needed */
void
ati_test_for_card_disappeared(void)
{
  if (aty_ld_le32(FIFO_STAT) == 0xffffffff)
    {
      unsigned short tmp;

      printf("ATI video card disappered -- re-mapping memory mapped "
	     "  registers.\n");
      PCIBIOS_READ_CONFIG_WORD(ati_pci_bus, ati_pci_devfn, PCI_COMMAND, &tmp);
      tmp |= PCI_COMMAND_MEMORY;
      PCIBIOS_WRITE_CONFIG_WORD(ati_pci_bus, ati_pci_devfn, PCI_COMMAND, tmp);
    }
}

#endif

static unsigned
bpp_to_depth(unsigned bpp)
{
  if (bpp <= 8)
    return DST_8BPP;
  else if (bpp <= 16)
    return DST_15BPP;
  else if (bpp <= 24)
    return DST_24BPP;
  else if (bpp <= 32)
    return DST_32BPP;

  return 0;
}

static void 
init_engine(void)
{
  unsigned pitch_value;

  aty128_wait_for_idle();

  /* 3D scaler not spoken here */
  aty_st_le32(SCALE_3D_CNTL, 0x00000000);

  aty128_reset_engine();

  pitch_value = hw_xres;
  if (hw_bits == 24)
    pitch_value *= 3;

  /* setup engine offset registers */
  aty128_wait_for_fifo(4);
  aty_st_le32(DEFAULT_OFFSET, 0x00000000);

  /* setup engine pitch registers */
  aty_st_le32(DEFAULT_PITCH, pitch_value/8);

  /* set the default scissor register to max dimensions */
  aty128_wait_for_fifo(1);
  aty_st_le32(DEFAULT_SC_BOTTOM_RIGHT, (0x1FFF << 16) | 0x1FFF);

  /* set the drawing controls registers */
  aty128_wait_for_fifo(1);
  aty_st_le32(DP_GUI_MASTER_CNTL,
	      GMC_SRC_PITCH_OFFSET_DEFAULT	|
	      GMC_DST_PITCH_OFFSET_DEFAULT	|
	      GMC_SRC_CLIP_DEFAULT		|
	      GMC_DST_CLIP_DEFAULT		|
	      GMC_BRUSH_SOLIDCOLOR		|
	      (bpp_to_depth(hw_bits) << 8)	|
	      GMC_SRC_DSTCOLOR			|
	      GMC_BYTE_ORDER_MSB_TO_LSB		|
	      GMC_DP_CONVERSION_TEMP_6500	|
	      ROP3_PATCOPY			|
	      GMC_DP_SRC_RECT			|
	      GMC_3D_FCN_EN_CLR			|
	      GMC_DST_CLR_CMP_FCN_CLEAR		|
	      GMC_AUX_CLIP_CLEAR		|
	      GMC_WRITE_MASK_SET);
  aty128_wait_for_fifo(8);

  /* clear the line drawing registers */
  aty_st_le32(DST_BRES_ERR, 0);
  aty_st_le32(DST_BRES_INC, 0);
  aty_st_le32(DST_BRES_DEC, 0);

  /* set brush color registers */
  aty_st_le32(DP_BRUSH_FRGD_CLR, 0xFFFFFFFF); /* white */
  aty_st_le32(DP_BRUSH_BKGD_CLR, 0x00000000); /* black */

  /* set source color registers */
  aty_st_le32(DP_SRC_FRGD_CLR, 0xFFFFFFFF);   /* white */
  aty_st_le32(DP_SRC_BKGD_CLR, 0x00000000);   /* black */

  /* default write mask */
  aty_st_le32(DP_WRITE_MASK, 0xFFFFFFFF);

  /* Wait for all the writes to be completed before returning */
  aty128_wait_for_idle();
}

#if 0

/* exported: wait until graphics engine is idle */
static void
ati_pan(int *x, int *y)
{
  unsigned bpp = (hw_bits + 1) & ~7;
  unsigned offset = ((*y * hw_xres + *x) * bpp / 64);
  unsigned margin_left   = *x;
  unsigned margin_right  = *x + hw_xres;

  if (bpp == 24)
    {
      /* In 24 bpp, the engine is in 8 bpp - this requires that all */
      /* horizontal coordinates and widths must be adjusted.
       * The offset value has to been corrected so that it is dividable 
       * by 3 because we have 3 bytes per pixel (packed). */
      offset = (offset / 3) * 3;
      *x = (64*offset)/bpp - *y*hw_xres;
      margin_left  = 3 * *x;
      margin_right = 3 * (*x + hw_xres);
    }
  
  wait_for_fifo(3);
  aty_st_le32(SC_TOP,    *y);
  aty_st_le32(SC_BOTTOM, *y+hw_yres-1);
  aty_st_le32(SC_LEFT,   margin_left);
  aty_st_le32(SC_RIGHT,  margin_right);
  aty_st_le32(CRTC_OFF_PITCH, offset | (hw_xres << 19));
}

#endif

static void 
ati128_bmove(struct l4con_vc *vc,
	     int sx, int sy, int width, int height, int dx, int dy)
{
  (void)vc;
  unsigned direction = 0;
  unsigned save_dp_datatype;

  if (hw_bits == 24)
    {
      /* In 24 bpp, the engine is in 8 bpp - this requires that all */
      /* horizontal coordinates and widths must be adjusted */
      sx *= 3;
      dx *= 3;
      width *= 3;
    }

  if (sy <= dy) 
    {
      dy += height - 1;
      sy += height - 1;
    } 
  else
    direction |= DST_Y_TOP_TO_BOTTOM;

  if (sx <= dx) 
    {
      dx += width - 1;
      sx += width - 1;
    } 
  else
    direction |= DST_X_LEFT_TO_RIGHT;

  aty128_wait_for_fifo(1);
  save_dp_datatype = aty_ld_le32(DP_DATATYPE);

  aty128_wait_for_fifo(7);
  aty_st_le32(CLR_CMP_CNTL, 0);
  aty_st_le32(DP_CNTL, direction);
  aty_st_le32(DP_DATATYPE, save_dp_datatype|bpp_to_depth(hw_bits)|SRC_DSTCOLOR);
  aty_st_le32(DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT);
  aty_st_le32(SRC_Y_X, (sy << 16) | sx);
  aty_st_le32(DST_Y_X, (dy << 16) | dx);
  aty_st_le32(DST_HEIGHT_WIDTH, (height << 16) | width);
  blitter_may_be_busy = 1;

  aty128_wait_for_fifo(1);
  aty_st_le32(DP_DATATYPE, save_dp_datatype);
}


static void 
ati128_fill(struct l4con_vc *vc,
	    int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  if (hw_bits == 24)
    {
      /* In 24 bpp, the engine is in 8 bpp - this requires that all */
      /* horizontal coordinates and widths must be adjusted */
      sx *= 3;
      width *= 3;
    }

  aty128_wait_for_fifo(5);
  aty_st_le32(DP_BRUSH_FRGD_CLR, color);
  aty_st_le32(DP_CNTL, DST_X_LEFT_TO_RIGHT|DST_Y_TOP_TO_BOTTOM);
  aty_st_le32(DP_MIX, ROP3_PATCOPY | DP_SRC_RECT);
  aty_st_le32(DST_Y_X, (sy << 16) | sx);
  aty_st_le32(DST_HEIGHT_WIDTH, (height << 16) | width);

  blitter_may_be_busy = 1;
}


static void
ati128_sync(void)
{
  if (blitter_may_be_busy)
    aty128_wait_for_idle();
}


static int
ati128_probe(unsigned int bus, unsigned int devfn,
	     const struct pci_device_id *dev, con_accel_t *accel)
{
  unsigned int addr;
  unsigned int vid_addr;
  unsigned short tmp;

  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_0, &addr);
  if ((addr & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO)
    PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_1, &addr);

  if (!addr)
    return -L4_ENODEV;

  vid_addr = addr & PCI_BASE_ADDRESS_MEM_MASK;

  PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_COMMAND, &tmp);
  if (!(tmp & PCI_COMMAND_MEMORY)) 
    {
      tmp |= PCI_COMMAND_MEMORY;
      PCIBIOS_WRITE_CONFIG_WORD(bus, devfn, PCI_COMMAND, tmp);
    }

  PCIBIOS_READ_CONFIG_DWORD(bus, devfn, PCI_BASE_ADDRESS_2, &addr);
  addr &= PCI_BASE_ADDRESS_MEM_MASK;

  // check if we have enough memory
  if (hw_vid_mem_size < 7*1024*1024)
    {
      printf("ati128: graphics memory size less than 8MB (%ldkB)\n",
	     hw_vid_mem_size/1024);
      return -L4_ENODEV;
    }

  if (map_io_mem(addr, 0x2000, 0, "ctrl", &ati128_regbase)<0)
    return -L4_ENODEV;

  if (map_io_mem(vid_addr, hw_vid_mem_size, 1, "video",
		 (l4_addr_t *)&hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  ati_pci_bus = bus;
  ati_pci_devfn = devfn;

  printf("Found ATI %s\n", ati128_ids[dev->driver_data & 0xf]);

  init_engine();

  accel->copy = ati128_bmove;
  accel->fill = ati128_fill;
  accel->sync = ati128_sync;
//  accel->pan  = ati_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  ati128_vid_init(accel);

  return 0;
}

void
ati128_register(void)
{
  pci_register(ati128_pci_tbl, ati128_probe);
}
