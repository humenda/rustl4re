/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef L4_CXX_UART_OF_H__
#define L4_CXX_UART_OF_H__

#include "uart_base.h"
#include <stdarg.h>
#include <string.h>
#include <l4/drivers/of.h>

namespace L4
{
  class Uart_of : public Uart, public L4_drivers::Of
  {
  private:
    ihandle_t  _serial;

  public:
    Uart_of()
      : Of(), _serial(0) {}
    bool startup(Io_register_block const *);

    void shutdown();
    bool change_mode(Transfer_mode m, Baud_rate r);
    int get_char(bool blocking = true) const;
    int char_avail() const;
    void out_char(char c) const;
    int write(char const *s, unsigned long count) const;
  };
};

#endif
