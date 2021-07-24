/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/drivers/hw_register_block>

#include "debug.h"

namespace {

struct Register_block_dummy : L4drivers::Register_block_impl<Register_block_dummy, 64>
{
  template< typename T >
  T read(l4_addr_t reg) const
  {
    d_printf(DBG_ERR, "ERROR: dummy device register read: reg=0x%lx (size=%zd)\n",
             reg, sizeof(T));
    return T(0);
  }

  template< typename T >
  void write(T value, l4_addr_t reg) const
  {
    d_printf(DBG_ERR, "ERROR: dummy device register write: reg=0x%lx (size=%zd) value=%lx\n",
             reg, sizeof(T), (unsigned long)value);
  }
};

static Register_block_dummy _dummy;

}

L4drivers::Register_block<64> dummy_register_block(&_dummy);
