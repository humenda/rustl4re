/*!
 * \file	ati.c
 * \brief	Hardware Acceleration for ATI Mach64 cards
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

/* most stuff taken from Linux 2.2.21: driver/video/atyfb.c */

#include <stdio.h>

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/sys/consts.h>
#include <l4/util/util.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "ati.h"
#include "aty.h"

struct aty_features 
{
  unsigned short pci_id;
  unsigned short chip_type;
  const char *name;
};

static struct aty_features aty_features[] =
{
    /* mach64GX family */
    { 0x4758, 0x00d7, "mach64GX (ATI888GX00)" },
    { 0x4358, 0x0057, "mach64CX (ATI888CX00)" },

    /* mach64CT family */
    { 0x4354, 0x4354, "mach64CT (ATI264CT)" },
    { 0x4554, 0x4554, "mach64ET (ATI264ET)" },

    /* mach64CT family / mach64VT class */
    { 0x5654, 0x5654, "mach64VT (ATI264VT)" },
    { 0x5655, 0x5655, "mach64VTB (ATI264VTB)" },
    { 0x5656, 0x5656, "mach64VT4 (ATI264VT4)" },

    /* mach64CT family / mach64GT (3D RAGE) class */
    { 0x4c42, 0x4c42, "3D RAGE LT PRO (AGP)" },
    { 0x4c44, 0x4c44, "3D RAGE LT PRO" },
    { 0x4c47, 0x4c47, "3D RAGE LT-G" },
    { 0x4c49, 0x4c49, "3D RAGE LT PRO" },
    { 0x4c50, 0x4c50, "3D RAGE LT PRO" },
    { 0x4c54, 0x4c54, "3D RAGE LT" },
    { 0x4752, 0x4752, "3D RAGE (XL)" },
    { 0x4754, 0x4754, "3D RAGE (GT)" },
    { 0x4755, 0x4755, "3D RAGE II+ (GTB)" },
    { 0x4756, 0x4756, "3D RAGE IIC (PCI)" },
    { 0x4757, 0x4757, "3D RAGE IIC (AGP)" },
    { 0x475a, 0x475a, "3D RAGE IIC (AGP)" },
    { 0x4742, 0x4742, "3D RAGE PRO (BGA, AGP)" },
    { 0x4744, 0x4744, "3D RAGE PRO (BGA, AGP, 1x only)" },
    { 0x4749, 0x4749, "3D RAGE PRO (BGA, PCI)" },
    { 0x4750, 0x4750, "3D RAGE PRO (PQFP, PCI)" },
    { 0x4751, 0x4751, "3D RAGE PRO (PQFP, PCI, limited 3D)" },
    { 0x4c4d, 0x4c4d, "3D RAGE Mobility (PCI)" },
    { 0x4c4e, 0x4c4e, "3D RAGE Mobility (AGP)" },
};

static const struct pci_device_id ati_pci_tbl[] __init =
{
    {PCI_VENDOR_ID_ATI, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

l4_addr_t ati_regbase = 0;
unsigned blitter_may_be_busy = 0;
int ati_supports_planar = 0;

static unsigned ati_pci_bus = 0;
static unsigned ati_pci_devfn = 0;
static unsigned short Gx = 0;
static unsigned char Rev = 0;

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

static void 
reset_engine(void)
{
  /* reset engine */
  aty_st_le32(GEN_TEST_CNTL,
	      aty_ld_le32(GEN_TEST_CNTL) & ~GUI_ENGINE_ENABLE);
  /* enable engine */
  aty_st_le32(GEN_TEST_CNTL,
	      aty_ld_le32(GEN_TEST_CNTL) | GUI_ENGINE_ENABLE);
  /* ensure engine is not locked up by clearing any FIFO or */
  /* HOST errors */
  aty_st_le32(BUS_CNTL, aty_ld_le32(BUS_CNTL) | BUS_HOST_ERR_ACK |
	      BUS_FIFO_ERR_ACK);
}

static void 
reset_GTC_3D_engine(void)
{
  aty_st_le32(SCALE_3D_CNTL, 0xc0);
  l4_sleep(GTC_3D_RESET_DELAY);
  aty_st_le32(SETUP_CNTL, 0x00);
  l4_sleep(GTC_3D_RESET_DELAY);
  aty_st_le32(SCALE_3D_CNTL, 0x00);
  l4_sleep(GTC_3D_RESET_DELAY);
}

static void 
init_engine(void)
{
  unsigned pitch_value;
  unsigned dp_pix_width, dp_chain_mask;

  /* determine modal information from global mode structure */
  pitch_value = hw_xres;

  if (hw_bits <= 8)
    {
      hw_bits = 8;
      dp_pix_width = HOST_8BPP | SRC_8BPP | DST_8BPP | BYTE_ORDER_LSB_TO_MSB;
      dp_chain_mask = 0x8080;
    }
  else if (hw_bits <= 16)
    {
      hw_bits = 16;
      dp_pix_width = HOST_15BPP | SRC_15BPP | DST_15BPP | BYTE_ORDER_LSB_TO_MSB;
      dp_chain_mask = 0x4210;
    }
  else if (hw_bits <= 24)
    {
      hw_bits = 24;
      /* In 24 bpp, the engine is in 8 bpp - this requires that all */
      /* horizontal coordinates and widths must be adjusted */
      pitch_value *= 3;
      dp_pix_width = HOST_8BPP | SRC_8BPP | DST_8BPP | BYTE_ORDER_LSB_TO_MSB;
      dp_chain_mask = 0x8080;
    }
  else
    {
      hw_bits = 32;
      dp_pix_width = HOST_32BPP | SRC_32BPP | DST_32BPP | BYTE_ORDER_LSB_TO_MSB;
      dp_chain_mask = 0x8080;
    }

  /* On GTC (RagePro), we need to reset the 3D engine first */
  if (Gx == LB_CHIP_ID || Gx == LD_CHIP_ID || Gx == LI_CHIP_ID ||
      Gx == LP_CHIP_ID || Gx == GB_CHIP_ID || Gx == GD_CHIP_ID ||
      Gx == GI_CHIP_ID || Gx == GP_CHIP_ID || Gx == GQ_CHIP_ID ||
      Gx == LM_CHIP_ID || Gx == LN_CHIP_ID)
    reset_GTC_3D_engine();

  /* Reset engine, enable, and clear any engine errors */
  reset_engine();

  /* Ensure that vga page pointers are set to zero - the upper */
  /* page pointers are set to 1 to handle overflows in the */
  /* lower page */
  aty_st_le32(MEM_VGA_WP_SEL, 0x00010000);
  aty_st_le32(MEM_VGA_RP_SEL, 0x00010000);

  /* ---- Setup standard engine context ---- */

  /* All GUI registers here are FIFOed - therefore, wait for */
  /* the appropriate number of empty FIFO entries */
  wait_for_fifo(14);

  /* enable all registers to be loaded for context loads */
  aty_st_le32(CONTEXT_MASK, 0xFFFFFFFF);

  /* set destination pitch to modal pitch, set offset to zero */
  aty_st_le32(DST_OFF_PITCH, (pitch_value / 8) << 22);

  /* zero these registers (set them to a known state) */
  aty_st_le32(DST_Y_X, 0);
  aty_st_le32(DST_HEIGHT, 0);
  aty_st_le32(DST_BRES_ERR, 0);
  aty_st_le32(DST_BRES_INC, 0);
  aty_st_le32(DST_BRES_DEC, 0);

  /* set destination drawing attributes */
  aty_st_le32(DST_CNTL, DST_LAST_PEL | DST_Y_TOP_TO_BOTTOM |
	      DST_X_LEFT_TO_RIGHT);

  /* set source pitch to modal pitch, set offset to zero */
  aty_st_le32(SRC_OFF_PITCH, (pitch_value / 8) << 22);

  /* set these registers to a known state */
  aty_st_le32(SRC_Y_X, 0);
  aty_st_le32(SRC_HEIGHT1_WIDTH1, 1);
  aty_st_le32(SRC_Y_X_START, 0);
  aty_st_le32(SRC_HEIGHT2_WIDTH2, 1);

  /* set source pixel retrieving attributes */
  aty_st_le32(SRC_CNTL, SRC_LINE_X_LEFT_TO_RIGHT);

  /* set host attributes */
  wait_for_fifo(13);
  aty_st_le32(HOST_CNTL, 0);

  /* set pattern attributes */
  aty_st_le32(PAT_REG0, 0);
  aty_st_le32(PAT_REG1, 0);
  aty_st_le32(PAT_CNTL, 0);

  /* set scissors to modal size */
  aty_st_le32(SC_LEFT, 0);
  aty_st_le32(SC_TOP, 0);
  aty_st_le32(SC_BOTTOM, hw_yres-1);
  aty_st_le32(SC_RIGHT, pitch_value-1);

  /* set background color to minimum value (usually BLACK) */
  aty_st_le32(DP_BKGD_CLR, 0);

  /* set foreground color to maximum value (usually WHITE) */
  aty_st_le32(DP_FRGD_CLR, 0xFFFFFFFF);

  /* set write mask to effect all pixel bits */
  aty_st_le32(DP_WRITE_MASK, 0xFFFFFFFF);
  
  /* set foreground mix to overpaint and background mix to */
  /* no-effect */
  aty_st_le32(DP_MIX, FRGD_MIX_S | BKGD_MIX_D);

  /* set primary source pixel channel to foreground color */
  /* register */
  aty_st_le32(DP_SRC, FRGD_SRC_FRGD_CLR);

  /* set compare functionality to false (no-effect on */
  /* destination) */
  wait_for_fifo(3);
  aty_st_le32(CLR_CMP_CLR, 0);
  aty_st_le32(CLR_CMP_MASK, 0xFFFFFFFF);
  aty_st_le32(CLR_CMP_CNTL, 0);

  /* set pixel depth */
  wait_for_fifo(2);
  aty_st_le32(DP_PIX_WIDTH, dp_pix_width);
  aty_st_le32(DP_CHAIN_MASK, dp_chain_mask);

  wait_for_fifo(5);
  aty_st_le32(SCALE_3D_CNTL, 0);
  aty_st_le32(Z_CNTL, 0);
  aty_st_le32(CRTC_INT_CNTL, aty_ld_le32(CRTC_INT_CNTL) & ~0x20);
  aty_st_le32(GUI_TRAJ_CNTL, 0x100023);

  /* insure engine is idle before leaving */
  wait_for_idle();
}

static inline void 
draw_rect(short x, short y, unsigned short width, unsigned short height)
{
  /* perform rectangle fill */
  wait_for_fifo(2);
  aty_st_le32(DST_Y_X, (x << 16) | y);
  aty_st_le32(DST_HEIGHT_WIDTH, (width << 16) | height);
  blitter_may_be_busy = 1;
}

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

static void 
ati_bmove(struct l4con_vc *vc,
	  int sx, int sy, int width, int height, int dx, int dy)
{
  (void)vc;
  unsigned direction = DST_LAST_PEL;

  if (hw_bits == 24)
    {
      /* In 24 bpp, the engine is in 8 bpp - this requires that all */
      /* horizontal coordinates and widths must be adjusted */
      sx *= 3;
      dx *= 3;
      width *= 3;
    }

  if (sy < dy) 
    {
      dy += height - 1;
      sy += height - 1;
    } 
  else
    direction |= DST_Y_TOP_TO_BOTTOM;
  
  if (sx < dx) 
    {
      dx += width - 1;
      sx += width - 1;
    } 
  else
    direction |= DST_X_LEFT_TO_RIGHT;

  wait_for_fifo(4);
  aty_st_le32(DP_SRC, FRGD_SRC_BLIT);
  aty_st_le32(SRC_Y_X, (sx << 16) | sy);
  aty_st_le32(SRC_HEIGHT1_WIDTH1, (width << 16) | height);
  aty_st_le32(DST_CNTL, direction);
  draw_rect(dx, dy, width, height);
}

static void 
ati_fill(struct l4con_vc *vc,
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

  wait_for_fifo(3);
  aty_st_le32(DP_FRGD_CLR, color);
  aty_st_le32(DP_SRC, BKGD_SRC_BKGD_CLR | FRGD_SRC_FRGD_CLR | MONO_SRC_ONE);
  aty_st_le32(DST_CNTL, DST_LAST_PEL | DST_Y_TOP_TO_BOTTOM |
	      DST_X_LEFT_TO_RIGHT);
  draw_rect(sx, sy, width, height);
}

static void
ati_sync(void)
{
  if (blitter_may_be_busy)
    wait_for_idle();
}

static int
ati_init(void)
{
  unsigned chip_id, j;
  const char *chipname = NULL;
  
  chip_id = aty_ld_le32(CONFIG_CHIP_ID);
  Gx = chip_id & CFG_CHIP_TYPE;
  Rev = (chip_id & CFG_CHIP_REV)>>24;
  for (j = 0; j < (sizeof(aty_features)/sizeof(*aty_features)); j++)
    if (aty_features[j].chip_type == Gx) 
      {
	chipname = aty_features[j].name;
	break;
      }
  
  if (!chipname)
    return 0;

  printf("Found ATI %s [0x%04x rev 0x%02x] (PCI %02x/%02x)\n", 
        chipname, Gx, Rev, ati_pci_bus, ati_pci_devfn);

  return 1;
}

static int
ati_probe(unsigned int bus, unsigned int devfn,
	  const struct pci_device_id *dev, con_accel_t *accel)
{
  unsigned int addr;
  unsigned short tmp;

  (void)dev;

  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_0, &addr);
  if ((addr & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO)
    PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_1, &addr);

  if (!addr)
    return -L4_ENODEV;

  addr &= PCI_BASE_ADDRESS_MEM_MASK;

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
      printf("ati: graphics memory size less than 8MB (%ldkB)\n",
	  hw_vid_mem_size/1024);
      return -L4_ENODEV;
    }

  if (map_io_mem(addr, 0x1000, 0, "ctrl", &ati_regbase)<0)
    return -L4_ENODEV;

  ati_pci_bus = bus;
  ati_pci_devfn = devfn;
  ati_regbase += 0x400;

  if (!ati_init())
    {
      unmap_io_mem(addr, 0x1000, "ctrl", ati_regbase-0x400);
      return -L4_ENODEV;
    }

  init_engine();

  if (map_io_mem(hw_vid_mem_addr, hw_vid_mem_size, 1, "video",
		 &hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  accel->copy = ati_bmove;
  accel->fill = ati_fill;
  accel->sync = ati_sync;
  accel->pan  = ati_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  ati_vid_init(accel);

  return 0;
}

void
ati_register(void)
{
  pci_register(ati_pci_tbl, ati_probe);
}

