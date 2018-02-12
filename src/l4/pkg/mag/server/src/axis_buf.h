/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once
#include <l4/sys/types.h>

namespace Mag_server
{

class Axis_buf
{
private:
  unsigned char s;
  l4_int32_t v[4];

public:
  Axis_buf(unsigned char _s) : s(_s-1) { assert ((_s & s) == 0); }
  l4_int32_t get(unsigned char idx) const { return v[idx & s]; }
  void set(unsigned char idx, l4_int32_t val) { v[idx & s] = val; }
  void copy(unsigned char idx1, unsigned char idx2)
  { set(idx1, get(idx2)); }
  l4_int32_t __getitem__(unsigned char idx) const { return v[idx & s]; }
  void __setitem__(unsigned char idx, l4_int32_t val) { v[idx & s] = val; }
};

}
