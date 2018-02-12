/*
 * (c) 2009-2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "uart_dummy.h"

namespace L4
{
  bool Uart_dummy::startup(Io_register_block const *)
  { return true; }

  void Uart_dummy::shutdown()
  {}

  bool Uart_dummy::change_mode(Transfer_mode, Baud_rate)
  { return true; }

  int Uart_dummy::get_char(bool /*blocking*/) const
  { return 0; }

  int Uart_dummy::char_avail() const
  { return false; }

  void Uart_dummy::out_char(char c) const
  { (void)c; }

  int Uart_dummy::write(char const *s, unsigned long count) const
  { (void)s; (void)count; return 0; }
};
