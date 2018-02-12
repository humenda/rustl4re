/*!
 * \file	matrox_regs.h
 * \brief	Matrox Gxx driver
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

#ifndef __MATROX_REGS_H_
#define __MATROX_REGS_H_

extern l4_addr_t mga_mmio_vbase;


static inline l4_uint8_t
mga_readb(l4_addr_t va, unsigned offs) 
{
  return *(volatile l4_uint8_t*)(va + offs);
}

static inline l4_uint16_t
mga_readw(l4_addr_t va, unsigned offs) 
{
  return *(volatile l4_uint16_t*)(va + offs);
}

static inline l4_uint32_t
mga_readl(l4_addr_t va, unsigned offs) 
{
  return *(volatile l4_uint32_t*)(va + offs);
}

static inline void
mga_writeb(l4_addr_t va, unsigned offs, unsigned char value) 
{
  *(volatile l4_uint8_t*)(va + offs) = value;
}

static inline void
mga_writew(l4_addr_t va, unsigned offs, unsigned short value) 
{
  *(volatile l4_uint16_t*)(va + offs) = value;
}

static inline void
mga_writel(l4_addr_t va, unsigned offs, unsigned value) 
{
  *(volatile l4_uint32_t*)(va + offs) = value;
}

#define mga_inb(addr)      mga_readb(mga_mmio_vbase, (addr))
#define mga_inl(addr)      mga_readl(mga_mmio_vbase, (addr))
#define mga_outb(addr,val) mga_writeb(mga_mmio_vbase, (addr), (val))
#define mga_outw(addr,val) mga_writew(mga_mmio_vbase, (addr), (val))
#define mga_outl(addr,val) mga_writel(mga_mmio_vbase, (addr), (val))

#define mga_readr(port,idx)     (mga_outb((port),(idx)), mga_inb((port)+1))
#define mga_setr(addr,port,val)  mga_outw(addr, ((val)<<8) | (port))

#define mga_fifo(n)	do {} while ((mga_inl(M_FIFOSTATUS) & 0xFF) < (n))
#define WaitTillIdle()	do {} while (mga_inl(M_STATUS) & 0x10000)

#endif

