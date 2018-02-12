/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "panic.h"
#include <l4/util/l4_macros.h>

#include "region.h"
#include "module.h"

unsigned long long
Region_list::find_free(Region const &search, unsigned long long _size,
                       unsigned align) const
{
  unsigned long long start = search.begin();
  unsigned long long end   = search.end();
  while (1)
    {
      start = (start + (1ULL << align) -1) & ~((1ULL << align)-1);

      if (start + _size - 1 > end)
        return 0;

      if (0)
        printf("try start %p\n", (void *)start);

      Region *z = find(Region::start_size(start, _size));
      if (!z)
        return start;

      start = z->end() + 1;
    }
}

unsigned long long
Region_list::find_free_rev(Region const &search, unsigned long long _size,
                           unsigned align) const
{
  unsigned long long start = search.begin();
  unsigned long long end   = search.end();
  while (1)
    {
      end -= _size - 1;
      end &= ~((1ULL << align)-1);

      if (end < start)
        return 0;

      if (0)
        printf("try start %p\n", (void *)end);

      Region *z = find(Region::start_size(end, _size));
      if (!z)
        return end;

      end = z->begin() - 1;
    }
}

void
Region_list::add_nolimitcheck(Region const &region, bool may_overlap)
{
  Region const *r;

  /* Do not add empty regions */
  if (region.begin() == region.end())
    return;

  if (_end >= _max)
    {
      // try to merge adjacent regions to gain space
      optimize();

      if (_end >= _max)
        panic("Bootstrap: %s: Region overflow\n", __func__);
    }

  if (!may_overlap && (r = find(region)))
    {
      printf("  New region for list %s:\t", _name);
      region.vprint();
      printf("  overlaps with:         \t");
      r->vprint();

      dump();
      panic("region overlap");
    }

  *_end = region;
  ++_end;
  _combined_size += region.size();
}

void
Region_list::add(Region const &region, bool may_overlap)
{
  Region mem = region;

  if (mem.invalid())
    {
      printf("  WARNING: trying to add invalid region to %s list.\n", _name);
      return;
    }

  if (mem.begin() > _address_limit)
    {
      printf("  Dropping '%s' region ", _name);
      mem.print();
      printf(" due to %lld MB address limit\n", _address_limit >> 20);
      return;
    }

  if (mem.end() >= _address_limit)
    {
      printf("  Limiting '%s' region ", _name);
      mem.print();
      mem.end(_address_limit - 1);
      printf(" to ");
      mem.print();
      printf(" due to %lld MB address limit\n", _address_limit >> 20);

    }

  if (_combined_size >= _max_combined_size)
    {
      printf("  Dropping '%s' region ", _name);
      mem.print();
      printf(" due to %lld MB size limit\n", _max_combined_size >> 20);
      return;
    }

  if (_combined_size + mem.size() > _max_combined_size)
    {
      printf("  Limiting '%s' region ", _name);
      mem.print();
      mem.end(mem.begin() + _max_combined_size - _combined_size - 1);
      printf(" to ");
      mem.print();
      printf(" due to %lld MB size limit\n", _max_combined_size >> 20);
    }

  add_nolimitcheck(mem, may_overlap);
}

Region *
Region_list::find(Region const &o) const
{
  for (Region *c = _reg; c < _end; ++c)
    if (c->overlaps(o))
      return c;

  return 0;
}

Region *
Region_list::contains(Region const &o)
{
  for (Region *c = _reg; c < _end; ++c)
    if (c->contains(o))
      return c;

  return 0;
}

void
Region::print() const
{
  printf("  [%9llx, %9llx] {%9llx}", begin(), end(), size());
}

void
Region::vprint() const
{
  static char const *types[] = {"Gap   ", "Kern  ", "Sigma0",
                                "Boot  ", "Root  ", "Arch  ", "Ram   ",
                                "Info  " };
  printf("  ");
  print();
  printf(" %s ", types[type()]);
  if (name())
    {
      if (*name() == '.')
	printf("%s", name() + 1);
      else
	print_module_name(name(), "");
    }
  putchar('\n');
}

void
Region_list::dump()
{
  Region const *i;
  Region const *j;
  unsigned long long min, mark = 0;

  printf("Regions of list '%s'\n", _name);
  for (i = _reg; i < _end; ++i)
    {
      min = ~0;
      Region const *min_idx = 0;
      for (j = _reg; j < _end; ++j)
	if (j->begin() < min && j->begin() >= mark)
	  {
	    min     = j->begin();
	    min_idx = j;
	  }
      if (!min_idx)
        {
          i->vprint();
          continue;
        }
      min_idx->vprint();
      mark = min_idx->begin() + 1;
    }
}

void
Region_list::swap(Region *a, Region *b)
{
  Region t = *a; *a = *b; *b = t;
}

void
Region_list::sort()
{
  if (end() - begin() < 2)
    return;
  bool swapped;

  Region *e = end() - 1;

  do
    {
      swapped = false;
      for (Region *c = begin(); c < e; ++c)
	{
	  Region *n = c; ++n;
	  if (*n < *c)
	    {
	      swap(c,n);
	      swapped = true;
	    }
	}
    }
  while (swapped);
}

Region *
Region_list::remove(Region *r)
{
  memmove(r, r+1, (end() - r - 1)*sizeof(Region));
  --_end;
  return r;
}

void
Region_list::optimize()
{
  sort();
  Region *c = begin();
  while (c < end())
    {
      Region *n = c; ++n;
      if (n == end())
	return;

      if (n->type() == c->type() && n->sub_type() == c->sub_type()
	  && n->name() == c->name() &&
	  l4_round_page(c->end()) >= l4_trunc_page(n->begin()))
	{
	  c->end(n->end());
	  remove(n);
	}
      else
	++c;
    }
}

bool
Region_list::sub(Region const &r)
{
  Region *c = contains(r);
  if (!c)
    return false;

  if (c->begin() == r.begin() && c->end() == r.end())
    {
      remove(c);
      return true;
    }

  if (c->begin() == r.begin())
    {
      c->begin(r.end() + 1);
      return true;
    }

  if (c->end() == r.end())
    {
      c->end(r.begin() - 1);
      return true;
    }

  Region tail(*c);
  tail.begin(r.end() + 1);
  c->end(r.begin() - 1);
  add(tail);
  return true;
}
