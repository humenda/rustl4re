/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "vcon_fe.h"
#include <l4/re/error_helper>

Vcon_fe::Vcon_fe(L4::Cap<L4::Vcon> con, L4Re::Util::Object_registry *r)
: Vcon_fe_base(con, r)
{
  L4Re::chksys(_vcon->bind(0, obj_cap()),
               "binding to input IRQ");

  l4_vcon_attr_t attr = { 0, 0, 0 };
  _vcon->get_attr(&attr);
  attr.l_flags &= ~(L4_VCON_ECHO | L4_VCON_ICANON);
  attr.o_flags &= ~(L4_VCON_ONLCR | L4_VCON_OCRNL | L4_VCON_ONLRET);
  attr.i_flags &= ~(L4_VCON_INLCR | L4_VCON_IGNCR | L4_VCON_ICRNL);
  _vcon->set_attr(&attr);

  handle_pending_input();
}
