/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

//#include <l4/scout-gfx/layout>
#include "layout_internal.h"

#include <vector>
#include <algorithm>

#include <inttypes.h>

namespace Scout_gfx {

namespace {

typedef int64_t Fixed64;
static inline Fixed64 to_fixed(int i) { return (Fixed64)i * 256; }
static inline int f_round(Fixed64 i)
{ return (i % 256 < 128) ? i / 256 : 1 + i / 256; }

}

void
Layout_info::calc(Array &chain, int start, int count, int pos, int space,
                  int spacer)
{
  // from QT 4.5
  int c_hint = 0;
  int c_min = 0;
  int c_max = 0;
  int sum_stretch = 0;
  int sum_spacing = 0;

  bool grow = false; // anyone who really wants to grow?
  //    bool canShrink = false; // anyone who could be persuaded to shrink?

  bool all_empty_nonstretch = true;
  int pending_spacing = -1;
  int spacer_count = 0;
  int i;

  for (i = start; i < start + count; i++)
    {
      Layout_info *data = &chain[i];

      data->done = false;
      c_hint += data->preferred_size();
      c_min += data->min_sz;
      c_max += data->max_sz;
      sum_stretch += data->stretch;
      if (!data->empty)
	{
	  /*
	     Using pending_spacing, we ensure that the spacing for the last
	     (non-empty) item is ignored.
	   */
	  if (pending_spacing >= 0)
	    {
	      sum_spacing += pending_spacing;
	      ++spacer_count;
	    }
	  pending_spacing = data->effective_spacer(spacer);
	}
      grow = grow || data->exp || data->stretch > 0;
      all_empty_nonstretch = all_empty_nonstretch && !grow && data->empty;
    }

  int extraspace = 0;

  if (space < c_min + sum_spacing)
    {
      /*
	 Less space than min_sz; take from the biggest first
       */

      int min_size = c_min + sum_spacing;

      // shrink the spacers proportionally
      if (spacer >= 0)
	{
	  spacer = min_size > 0 ? spacer * space / min_size : 0;
	  sum_spacing = spacer * spacer_count;
	}

      std::vector<int> list;

      for (i = start; i < start + count; i++)
	list.push_back(chain[i].min_sz);

      std::sort(list.begin(), list.end());

      int space_left = space - sum_spacing;

      int sum = 0;
      int idx = 0;
      int space_used=0;
      int current = 0;
      while (idx < count && space_used < space_left)
	{
	  current = list.at(idx);
	  space_used = sum + current * (count - idx);
	  sum += current;
	  ++idx;
	}
      --idx;
      int deficit = space_used - space_left;

      int items = count - idx;
      /*
       * If we truncate all items to "current", we would get "deficit" too many pixels. Therefore, we have to remove
       * deficit/items from each item bigger than maxval. The actual value to remove is deficit_per_item + remainder/items
       * "rest" is the accumulated error from using integer arithmetic.
       */
      int deficit_per_item = deficit/items;
      int remainder = deficit % items;
      int maxval = current - deficit_per_item;

      int rest = 0;
      for (i = start; i < start + count; i++)
	{
	  int maxv = maxval;
	  rest += remainder;
	  if (rest >= items)
	    {
	      maxv--;
	      rest-=items;
	    }

	  Layout_info *data = &chain[i];
	  data->size = std::min(data->min_sz, maxv);
	  data->done = true;
	}
    }
  else if (space < c_hint + sum_spacing)
    {
      /*
	 Less space than preferred_size(), but more than min_sz.
	 Currently take space equally from each, as in Qt 2.x.
	 Commented-out lines will give more space to stretchier
	 items.
       */
      int n = count;
      int space_left = space - sum_spacing;
      int overdraft = c_hint - space_left;

      // first give to the fixed ones:
      for (i = start; i < start + count; i++)
	{
	  Layout_info *data = &chain[i];
	  if (!data->done
	      && data->min_sz >= data->preferred_size())
	    {
	      data->size = data->preferred_size();
	      data->done = true;
	      space_left -= data->preferred_size();
	      // sum_stretch -= data->stretch;
	      n--;
	    }
	}
      bool finished = n == 0;
      while (!finished)
	{
	  finished = true;
	  Fixed64 fp_over = to_fixed(overdraft);
	  Fixed64 fp_w = 0;

	  for (i = start; i < start+count; i++)
	    {
	      Layout_info *data = &chain[i];
	      if (data->done)
		continue;
	      // if (sum_stretch <= 0)
	      fp_w += fp_over / n;
	      // else
	      //    fp_w += (fp_over * data->stretch) / sum_stretch;
	      int w = f_round(fp_w);
	      data->size = data->preferred_size() - w;
	      fp_w -= to_fixed(w); // give the difference to the next
	      if (data->size < data->min_sz)
		{
		  data->done = true;
		  data->size = data->min_sz;
		  finished = false;
		  overdraft -= data->preferred_size() - data->min_sz;
		  // sum_stretch -= data->stretch;
		  n--;
		  break;
		}
	    }
	}
    }
  else
    { // extra space
      int n = count;
      int space_left = space - sum_spacing;
      // first give to the fixed ones, and handle non-expness
      for (i = start; i < start + count; i++)
	{
	  Layout_info *data = &chain[i];
	  if (!data->done
	      && (data->max_sz <= data->preferred_size()
		|| (grow && !data->exp && data->stretch == 0)
		|| (!all_empty_nonstretch && data->empty &&
		  !data->exp && data->stretch == 0)))
	    {
	      data->size = data->preferred_size();
	      data->done = true;
	      space_left -= data->size;
	      sum_stretch -= data->stretch;
	      n--;
	    }
	}
      extraspace = space_left;

      /*
	 Do a trial distribution and calculate how much it is off.
	 If there are more deficit pixels than surplus pixels, give
	 the minimum size items what they need, and repeat.
	 Otherwise give to the maximum size items, and repeat.

	 Paul Olav Tvete has a wonderful mathematical proof of the
	 correctness of this principle, but unfortunately this
	 comment is too small to contain it.
       */
      int surplus, deficit;
      do
	{
	  surplus = deficit = 0;
	  Fixed64 fp_space = to_fixed(space_left);
	  Fixed64 fp_w = 0;
	  for (i = start; i < start + count; i++)
	    {
	      Layout_info *data = &chain[i];
	      if (data->done)
		continue;
	      extraspace = 0;
	      if (sum_stretch <= 0)
		fp_w += fp_space / n;
	      else
		fp_w += (fp_space * data->stretch) / sum_stretch;
	      int w = f_round(fp_w);
	      data->size = w;
	      fp_w -= to_fixed(w); // give the difference to the next
	      if (w < data->preferred_size())
		{
		  deficit +=  data->preferred_size() - w;
		}
	      else if (w > data->max_sz)
		{
		  surplus += w - data->max_sz;
		}
	    }
	  if (deficit > 0 && surplus <= deficit)
	    {
	      // give to the ones that have too little
	      for (i = start; i < start+count; i++)
		{
		  Layout_info *data = &chain[i];
		  if (!data->done && data->size < data->preferred_size())
		    {
		      data->size = data->preferred_size();
		      data->done = true;
		      space_left -= data->preferred_size();
		      sum_stretch -= data->stretch;
		      n--;
		    }
		}
	    }
	  if (surplus > 0 && surplus >= deficit)
	    {
	      // take from the ones that have too much
	      for (i = start; i < start + count; i++)
		{
		  Layout_info *data = &chain[i];
		  if (!data->done && data->size > data->max_sz)
		    {
		      data->size = data->max_sz;
		      data->done = true;
		      space_left -= data->max_sz;
		      sum_stretch -= data->stretch;
		      n--;
		    }
		}
	    }
	}
      while (n > 0 && surplus != deficit);
      if (n == 0)
	extraspace = space_left;
    }

  /*
     As a last resort, we distribute the unwanted space equally
     among the spacers (counting the start and end of the chain). We
     could, but don't, attempt a sub-pixel allocation of the extra
     space.
   */
  int extra = extraspace / (spacer_count + 2);
  int p = pos + extra;
  for (i = start; i < start+count; i++)
    {
      Layout_info *data = &chain[i];
      data->pos = p;
      p += data->size;
      if (!data->empty)
	p += data->effective_spacer(spacer) + extra;
    }
}

}
