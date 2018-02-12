/*!
 * \file	radeon.c
 * \brief	Hardware Acceleration for ATI Radeon cards
 *
 * \date	10/2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* (c) 2005 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

/* most stuff taken from Linux 2.6.13: driver/video/radeon.c */

#include <stdio.h>

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/sys/consts.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>

#include "init.h"
#include "pci.h"
#include "iomem.h"
#include "radeon.h"
#include "radeon_reg.h"

#define PCI_DEVICE_ID_ATI_RADEON_QD	0x5144
#define PCI_DEVICE_ID_ATI_RADEON_QE	0x5145
#define PCI_DEVICE_ID_ATI_RADEON_QF	0x5146
#define PCI_DEVICE_ID_ATI_RADEON_QG	0x5147
/* Radeon RV100 (VE) */
#define PCI_DEVICE_ID_ATI_RADEON_QY	0x5159
#define PCI_DEVICE_ID_ATI_RADEON_QZ	0x515a
/* Radeon R200 (8500) */
#define PCI_DEVICE_ID_ATI_RADEON_QL	0x514c
#define PCI_DEVICE_ID_ATI_RADEON_QN	0x514e
#define PCI_DEVICE_ID_ATI_RADEON_QO	0x514f
#define PCI_DEVICE_ID_ATI_RADEON_Ql	0x516c
#define PCI_DEVICE_ID_ATI_RADEON_BB	0x4242
/* Radeon R200 (9100) */
#define PCI_DEVICE_ID_ATI_RADEON_QM	0x514d
/* Radeon RV200 (7500) */
#define PCI_DEVICE_ID_ATI_RADEON_QW	0x5157
#define PCI_DEVICE_ID_ATI_RADEON_QX	0x5158
/* Radeon NV-100 */
#define PCI_DEVICE_ID_ATI_RADEON_N1	0x5159
#define PCI_DEVICE_ID_ATI_RADEON_N2	0x515a
/* Radeon RV250 (9000) */
#define PCI_DEVICE_ID_ATI_RADEON_Id	0x4964
#define PCI_DEVICE_ID_ATI_RADEON_Ie	0x4965
#define PCI_DEVICE_ID_ATI_RADEON_If	0x4966
#define PCI_DEVICE_ID_ATI_RADEON_Ig	0x4967
/* Radeon RV280 (9200) */
#define PCI_DEVICE_ID_ATI_RADEON_Y_	0x5960
#define PCI_DEVICE_ID_ATI_RADEON_Ya	0x5961
#define PCI_DEVICE_ID_ATI_RADEON_Yd	0x5964
/* Radeon R300 (9500) */
#define PCI_DEVICE_ID_ATI_RADEON_AD	0x4144
/* Radeon R300 (9700) */
#define PCI_DEVICE_ID_ATI_RADEON_ND	0x4e44
#define PCI_DEVICE_ID_ATI_RADEON_NE	0x4e45
#define PCI_DEVICE_ID_ATI_RADEON_NF	0x4e46
#define PCI_DEVICE_ID_ATI_RADEON_NG	0x4e47
#define PCI_DEVICE_ID_ATI_RADEON_AE	0x4145
#define PCI_DEVICE_ID_ATI_RADEON_AF	0x4146
/* Radeon R350 (9800) */
#define PCI_DEVICE_ID_ATI_RADEON_NH	0x4e48
#define PCI_DEVICE_ID_ATI_RADEON_NI	0x4e49
/* Radeon RV350 (9600) */
#define PCI_DEVICE_ID_ATI_RADEON_AP	0x4150
#define PCI_DEVICE_ID_ATI_RADEON_AR	0x4152
/* Radeon M6 */
#define PCI_DEVICE_ID_ATI_RADEON_LY	0x4c59
#define PCI_DEVICE_ID_ATI_RADEON_LZ	0x4c5a
/* Radeon M7 */
#define PCI_DEVICE_ID_ATI_RADEON_LW	0x4c57
#define PCI_DEVICE_ID_ATI_RADEON_LX	0x4c58
/* Radeon M9 */
#define PCI_DEVICE_ID_ATI_RADEON_Ld	0x4c64
#define PCI_DEVICE_ID_ATI_RADEON_Le	0x4c65
#define PCI_DEVICE_ID_ATI_RADEON_Lf	0x4c66
#define PCI_DEVICE_ID_ATI_RADEON_Lg	0x4c67
/* Radeon */
#define PCI_DEVICE_ID_ATI_RADEON_RA	0x5144
#define PCI_DEVICE_ID_ATI_RADEON_RB	0x5145
#define PCI_DEVICE_ID_ATI_RADEON_RC	0x5146
#define PCI_DEVICE_ID_ATI_RADEON_RD	0x5147

enum radeon_chips
{
  RADEON_QD, RADEON_QE, RADEON_QF, RADEON_QG, RADEON_QY, RADEON_QZ, RADEON_LW,
  RADEON_LX, RADEON_LY, RADEON_LZ, RADEON_QL, RADEON_QN, RADEON_QO, RADEON_Ql,
  RADEON_BB, RADEON_QW, RADEON_QX, RADEON_Id, RADEON_Ie, RADEON_If, RADEON_Ig,
  RADEON_Ya, RADEON_Yd, RADEON_Ld, RADEON_Le, RADEON_Lf, RADEON_Lg, RADEON_ND,
  RADEON_NE, RADEON_NF, RADEON_NG, RADEON_QM
};

static const struct pci_device_id radeon_pci_tbl[] __init =
{
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QD, 0, 0, RADEON_QD},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QE, 0, 0, RADEON_QE},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QF, 0, 0, RADEON_QF},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QG, 0, 0, RADEON_QG},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QY, 0, 0, RADEON_QY},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QZ, 0, 0, RADEON_QZ},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LW, 0, 0, RADEON_LW},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LX, 0, 0, RADEON_LX},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LY, 0, 0, RADEON_LY},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_LZ, 0, 0, RADEON_LZ},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QL, 0, 0, RADEON_QL},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QN, 0, 0, RADEON_QN},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QO, 0, 0, RADEON_QO},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ql, 0, 0, RADEON_Ql},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_BB, 0, 0, RADEON_BB},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QW, 0, 0, RADEON_QW},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QX, 0, 0, RADEON_QX},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Id, 0, 0, RADEON_Id},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ie, 0, 0, RADEON_Ie},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_If, 0, 0, RADEON_If},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ig, 0, 0, RADEON_Ig},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ya, 0, 0, RADEON_Ya},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Yd, 0, 0, RADEON_Yd},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Ld, 0, 0, RADEON_Ld},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Le, 0, 0, RADEON_Le},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Lf, 0, 0, RADEON_Lf},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_Lg, 0, 0, RADEON_Lg},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_ND, 0, 0, RADEON_ND},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NE, 0, 0, RADEON_NE},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NF, 0, 0, RADEON_NF},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_NG, 0, 0, RADEON_NG},
    { PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RADEON_QM, 0, 0, RADEON_QM},
    { PCI_VENDOR_ID_ATI, 0x5B60,                      0, 0, 0},
    { 0, 0, 0, 0, 0}
};


#define INREG8(addr)	   *(volatile l4_uint8_t*)(mmio_base + addr)
#define OUTREG8(addr,val)  *(volatile l4_uint8_t*)(mmio_base + addr) = val
#define INREG(addr)        *(volatile l4_uint32_t*)(mmio_base + addr)
#define OUTREG(addr,val)   *(volatile l4_uint32_t*)(mmio_base + addr) = val

#define OUTPLL(addr,val)	\
	do {	\
		OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x0000003f) | 0x00000080); \
		OUTREG(CLOCK_CNTL_DATA, val); \
	} while(0)

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static l4_addr_t mmio_base = 0;
static int pitch;
static l4_uint32_t dp_gui_master_cntl;

static inline l4_uint32_t INPLL(l4_uint32_t addr)
{
  OUTREG8(CLOCK_CNTL_INDEX, addr & 0x0000003f);
  return (INREG(CLOCK_CNTL_DATA));
}

static inline void
radeon_engine_flush(void)
{
  int i;

  /* initiate flush */
  OUTREGP(RB2D_DSTCACHE_CTLSTAT, RB2D_DC_FLUSH_ALL, ~RB2D_DC_FLUSH_ALL);

  for (i=0; i < 2000000; i++)
    if (!(INREG(RB2D_DSTCACHE_CTLSTAT) & RB2D_DC_BUSY))
      break;
}

static inline void
radeon_fifo_wait(unsigned entries)
{
  int i;

  for (i=0; i<2000000; i++)
    if ((INREG(RBBM_STATUS) & 0x7f) >= entries)
      return;
}

static void
radeon_engine_idle(void)
{
  int i;

  /* ensure FIFO is empty before waiting for idle */
  radeon_fifo_wait (64);

  for (i=0; i<2000000; i++)
    if (((INREG(RBBM_STATUS) & GUI_ACTIVE)) == 0)
	{
	  radeon_engine_flush ();
	  return;
	}
}

static void
radeon_engine_reset(void)
{
  l4_uint32_t clock_cntl_index, mclk_cntl, rbbm_soft_reset;

  radeon_engine_flush ();

  clock_cntl_index = INREG(CLOCK_CNTL_INDEX);
  mclk_cntl = INPLL(MCLK_CNTL);

  OUTPLL(MCLK_CNTL, (mclk_cntl | FORCEON_MCLKA 
	                       | FORCEON_MCLKB
			       | FORCEON_YCLKA
			       | FORCEON_YCLKB
			       | FORCEON_MC
			       | FORCEON_AIC));
  rbbm_soft_reset = INREG(RBBM_SOFT_RESET);

  OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset | SOFT_RESET_CP
					  | SOFT_RESET_HI
					  | SOFT_RESET_SE
					  | SOFT_RESET_RE
					  | SOFT_RESET_PP
					  | SOFT_RESET_E2
					  | SOFT_RESET_RB);
  INREG(RBBM_SOFT_RESET);
  OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset & (l4_uint32_t) ~(SOFT_RESET_CP |
							    SOFT_RESET_HI |
							    SOFT_RESET_SE |
							    SOFT_RESET_RE |
							    SOFT_RESET_PP |
							    SOFT_RESET_E2 |
							    SOFT_RESET_RB));
  INREG(RBBM_SOFT_RESET);

  OUTPLL(MCLK_CNTL, mclk_cntl);
  OUTREG(CLOCK_CNTL_INDEX, clock_cntl_index);
  OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset);

  return;
}

static void
radeon_engine_init (void)
{
  l4_uint32_t temp;

  /* disable 3D engine */
  OUTREG(RB3D_CNTL, 0);

  radeon_engine_reset ();

  radeon_fifo_wait (1);
  OUTREG(RB2D_DSTCACHE_MODE, 0);

  radeon_fifo_wait (1);
  temp = INREG(DEFAULT_PITCH_OFFSET);
  OUTREG(DEFAULT_PITCH_OFFSET, ((temp & 0xc0000000) | (pitch << 0x16)));

  radeon_fifo_wait (1);
  OUTREGP(DP_DATATYPE, 0, ~HOST_BIG_ENDIAN_EN);

  radeon_fifo_wait (1);
  OUTREG(DEFAULT_SC_BOTTOM_RIGHT, (DEFAULT_SC_RIGHT_MAX |
	DEFAULT_SC_BOTTOM_MAX));

  switch (hw_bits)
    {
    case 8:  temp = DST_8BPP;  break;
    case 15: temp = DST_15BPP; break;
    case 16: temp = DST_16BPP; break;
    case 32: temp = DST_32BPP; break;
    default: temp = 0;         break;
    }

  dp_gui_master_cntl = (temp << 8) | GMC_CLR_CMP_CNTL_DIS;
  radeon_fifo_wait (1);
  OUTREG(DP_GUI_MASTER_CNTL, (dp_gui_master_cntl | GMC_BRUSH_SOLID_COLOR 
	                                         | GMC_SRC_DATATYPE_COLOR));

  radeon_fifo_wait (7);

  /* clear line drawing regs */
  OUTREG(DST_LINE_START, 0);
  OUTREG(DST_LINE_END, 0);

  /* set brush color regs */
  OUTREG(DP_BRUSH_FRGD_CLR, 0xffffffff);
  OUTREG(DP_BRUSH_BKGD_CLR, 0x00000000);

  /* set source color regs */
  OUTREG(DP_SRC_FRGD_CLR, 0xffffffff);
  OUTREG(DP_SRC_BKGD_CLR, 0x00000000);

  /* default write mask */
  OUTREG(DP_WRITE_MSK, 0xffffffff);

  radeon_engine_idle ();
}

static void
radeon_fill(struct l4con_vc *vc,
            int sx, int sy, int width, int height, unsigned color)
{
  (void)vc;
  radeon_fifo_wait(4);  

  OUTREG(DP_GUI_MASTER_CNTL, dp_gui_master_cntl | GMC_BRUSH_SOLID_COLOR
                                                | ROP3_P);
  OUTREG(DP_BRUSH_FRGD_CLR, color);
  OUTREG(DP_WRITE_MSK, 0xffffffff);
  OUTREG(DP_CNTL, (DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM));

  radeon_fifo_wait(2);  
  OUTREG(DST_Y_X, (sy << 16) | sx);
  OUTREG(DST_WIDTH_HEIGHT, (width << 16) | height);
}

static void
radeon_copy(struct l4con_vc *vc,
            int sx, int sy, int width, int height, int dx, int dy)
{
  int xdir = sx - dx, ydir = sy - dy;

  (void)vc;
  if (xdir < 0)
    {
      sx += width-1; 
      dx += width-1;
    }
  if (ydir < 0)
    {
      sy += height-1;
      dy += height-1;
    }

  radeon_fifo_wait(3);
  OUTREG(DP_GUI_MASTER_CNTL, dp_gui_master_cntl | GMC_BRUSH_NONE
						| GMC_SRC_DSTCOLOR
						| ROP3_S 
						| DP_SRC_SOURCE_MEMORY );
  OUTREG(DP_WRITE_MSK, 0xffffffff);
  OUTREG(DP_CNTL, (xdir>=0 ? DST_X_LEFT_TO_RIGHT : 0) |
		  (ydir>=0 ? DST_Y_TOP_TO_BOTTOM : 0));

  radeon_fifo_wait(3);
  OUTREG(SRC_Y_X, (sy << 16) | sx);
  OUTREG(DST_Y_X, (dy << 16) | dx);
  OUTREG(DST_HEIGHT_WIDTH, (height << 16) | width);
}

static void
radeon_pan(int *x, int *y)
{
  radeon_fifo_wait(2);
  OUTREG(CRTC_OFFSET, ((*y * hw_xres + *x) * hw_bits / 8) & ~7);
}

static void
radeon_sync(void)
{
  radeon_engine_idle();
}

static int
radeon_init(void)
{
  pitch = ((hw_xres * ((hw_bits + 1) / 8) + 0x3f) & ~(0x3f)) / 64;
  printf("Found ATI Radeon\n");

  return 1;
}

static int
radeon_probe(unsigned int bus, unsigned int devfn,
	     const struct pci_device_id *dev, con_accel_t *accel)
{
  unsigned int addr;
  unsigned short tmp;
  (void)dev;

  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, PCI_BASE_ADDRESS_0, &addr);
  if (!addr)
    return -L4_ENODEV;

  addr &= PCI_BASE_ADDRESS_MEM_MASK;

  PCIBIOS_READ_CONFIG_WORD (bus, devfn, PCI_COMMAND, &tmp);
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
      printf("radeon: graphics memory size less than 8MB (%ldkB)\n",
	     hw_vid_mem_size/1024);
      return -L4_ENODEV;
    }

  if (map_io_mem(addr, 0x4000, 0, "ctrl", (l4_addr_t *)&mmio_base)<0)
    return -L4_ENODEV;

  if (!radeon_init())
    {
      unmap_io_mem(addr, 0x4000, "ctrl", mmio_base);
      return -L4_ENODEV;
    }

  radeon_engine_init();

  if (map_io_mem(hw_vid_mem_addr, hw_vid_mem_size, 1, "video",
		 (l4_addr_t *)&hw_map_vid_mem_addr)<0)
    return -L4_ENODEV;

  accel->copy = radeon_copy;
  accel->fill = radeon_fill;
  accel->sync = radeon_sync;
  accel->pan  = radeon_pan;
  accel->caps = ACCEL_FAST_COPY | ACCEL_FAST_FILL;

  return 0;
}

void
radeon_register(void)
{
  pci_register(radeon_pci_tbl, radeon_probe);
}
