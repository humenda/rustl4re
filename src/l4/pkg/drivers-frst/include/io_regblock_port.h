/*
 * (c) 2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "io_regblock.h"

namespace L4
{
  class Io_register_block_port : public Io_register_block
  {
  public:
    Io_register_block_port(unsigned long base)
    : _base(base)
    {}

    unsigned long addr(unsigned long reg) const { return _base + reg; }

    unsigned char  read8(unsigned long reg) const
    {
      unsigned char val;
      asm volatile("inb %w1, %b0" : "=a" (val) : "Nd" (_base + reg));
      return val;
    }

    unsigned short read16(unsigned long reg) const
    {
      unsigned short val;
      asm volatile("inw %w1, %w0" : "=a" (val) : "Nd" (_base + reg));
      return val;
    }

    unsigned int   read32(unsigned long reg) const
    {
      unsigned int val;
      asm volatile("in %w1, %0" : "=a" (val) : "Nd" (_base + reg));
      return val;
    }

    void write8(unsigned long reg, unsigned char val) const
    { asm volatile("outb %b0, %w1" : : "a" (val), "Nd" (_base + reg)); }

    void write16(unsigned long reg, unsigned short val) const
    { asm volatile("outw %w0, %w1" : : "a" (val), "Nd" (_base + reg)); }

    void write32(unsigned long reg, unsigned int val) const
    { asm volatile("out %0, %w1" : : "a" (val), "Nd" (_base + reg)); }

    void delay() const
    { asm volatile ("outb %al,$0x80"); }

  private:
    unsigned long _base;
  };
}
