/*
 * (c) 2012 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

namespace L4 {

class Poll_timeout_counter
{
public:
  Poll_timeout_counter(unsigned counter_val)
  {
    set(counter_val);
  }

  void set(unsigned counter_val)
  {
    _c = counter_val;
  }

  bool test(bool expression = true)
  {
    if (!expression)
      return false;

    if (_c)
      {
        --_c;
        return true;
      }

    return false;
  }

  bool timed_out() const { return _c == 0; }

private:
  unsigned _c;
};

}
