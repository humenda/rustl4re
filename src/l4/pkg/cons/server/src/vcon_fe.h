/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "vcon_fe_base.h"

#include <l4/re/util/object_registry>

class Vcon_fe : public Vcon_fe_base
{
public:
  Vcon_fe(L4::Cap<L4::Vcon> con, L4Re::Util::Object_registry *r);
  int write(char const *buf, unsigned sz)
  { return do_write(buf, sz); }
};
