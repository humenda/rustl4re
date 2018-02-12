/*!
 * \file	savage_regs.h
 * \brief	S3 Savage driver
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

#ifndef __SAVAGE_REGS_H_
#define __SAVAGE_REGS_H_

#define savage_in16(addr)\
  (*((volatile l4_uint16_t*)(savage_mmio+(addr))))
#define savage_in32(addr)\
  (*((volatile l4_uint32_t*)(savage_mmio+(addr))))
#define savage_out16(addr,val)\
  (*((volatile l4_uint16_t*)(savage_mmio+(addr)))=(l4_uint16_t)(val))
#define savage_out32(addr,val)\
  (*((volatile l4_uint32_t*)(savage_mmio+(addr)))=(l4_uint32_t)(val))

#define vga_in8(addr)\
  (*((volatile l4_uint8_t*)(savage_mmio+0x8000+(addr))))
#define vga_in16(addr)\
  (*((volatile l4_uint16_t*)(savage_mmio+0x8000+(addr))))
#define vga_out8(addr,val)\
  (*((volatile l4_uint8_t*)(savage_mmio+0x8000+(addr)))=(l4_uint8_t)(val))
#define vga_out16(addr,val)\
  (*((volatile l4_uint16_t*)(savage_mmio+0x8000+(addr)))=(l4_uint16_t)(val))

typedef enum 
{
  S3_UNKNOWN = 0,
  S3_SAVAGE3D,
  S3_SAVAGE_MX,
  S3_SAVAGE4,
  S3_PROSAVAGE,
  S3_SUPERSAVAGE,
  S3_SAVAGE2000,
  S3_LAST
} savage_chipset;

#define S3_SAVAGE3D_SERIES(chip)  ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE_MX))
#define S3_SAVAGE4_SERIES(chip)   ((chip==S3_SAVAGE4) || (chip==S3_PROSAVAGE))
#define S3_SAVAGE_SERIES(chip)    ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE2000))
#define S3_SAVAGEMOB_SERIES(chip) ((chip==S3_SAVAGE_MX) || (chip==S3_SUPERSAVAGE))

extern l4_addr_t savage_mmio;
extern void (*SavageWaitIdle) (void);
extern void (*SavageWaitFifo) (unsigned space);
extern unsigned int savage_chip;

static inline void
savage_vertical_retrace_wait(void)
{
  vga_in8(0x3d4);
  vga_out8(0x3d4, 0x17);
  if (vga_in8(0x3d5) & 0x80)
    {
      while ((vga_in8(0x3da) & 0x08) == 0x08)
	;
      while ((vga_in8(0x3da) & 0x08) == 0x00)
	;
    }
}

#endif

