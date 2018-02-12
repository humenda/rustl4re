/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <vector>

namespace Scout_gfx {

struct Layout_info
{
  int pref_sz;
  int min_sz;
  int max_sz;
  int spacing;
  int stretch;

  unsigned exp : 1;
  unsigned empty : 1;

  unsigned done : 1;

  int pos;
  int size;

  void init(int stretch = 0, int min = 0)
  {
    spacing = 0;
    this->stretch = stretch;
    min_sz = min;
    pref_sz = min;
    max_sz = 100000;
    exp = false;
    empty = true;
  }

  int preferred_size() const
  { return stretch > 0 ? min_sz : pref_sz; }

  int effective_spacer(int uniform_spacer) const
  { return (uniform_spacer >= 0) ? uniform_spacer : spacing; }

  typedef std::vector<Layout_info> Array;
  static void calc(Array &a, int start, int cnt, int pos, int space,
                   int spacer = -1);
};

namespace {

static inline void max_exp_calc(int & max, bool &exp, bool &empty,
                                int boxmax, bool boxexp, bool boxempty)
{
  if (exp)
    {
      if (boxexp)
	max = std::max(max, boxmax);
    }
  else
    {
      if (boxexp || (empty && (!boxempty || max == 0)))
	max = boxmax;
      else if (empty == boxempty)
	max = std::min(max, boxmax);
    }
  exp = exp || boxexp;
  empty = empty && boxempty;
}

}

}
