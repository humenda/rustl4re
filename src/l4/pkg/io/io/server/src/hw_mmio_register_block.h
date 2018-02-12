/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "hw_register_block.h"

namespace Hw {

class Mmio_register_block_base
{
private:
  l4_addr_t _base;
  l4_addr_t _shift;

public:
  explicit Mmio_register_block_base(l4_addr_t base = 0, l4_addr_t shift = 0)
  : _base(base), _shift(shift) {}

  template< typename T >
  T read(l4_addr_t reg) const
  { return *reinterpret_cast<volatile const T *>(_base + (reg << _shift)); }

  template< typename T >
  void write(T value, l4_addr_t reg) const
  { *reinterpret_cast<volatile T *>(_base + (reg << _shift)) = value; }

  void set_base(l4_addr_t base) { _base = base; }
  void set_shift(l4_addr_t shift) { _shift = shift; }
};

template< unsigned MAX_BITS = 32 >
struct Mmio_register_block :
  Register_block_impl<Mmio_register_block<MAX_BITS>, MAX_BITS>,
  Mmio_register_block_base
{
  explicit Mmio_register_block(l4_addr_t base = 0, l4_addr_t shift = 0)
  : Mmio_register_block_base(base, shift) {}
};

}


