/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/kip>
#include <l4/re/env>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "debug.h"
#include "cfg.h"
#include "phys_space.h"

void *operator new (size_t sz, cxx::Nothrow const &) throw()
{ return malloc(sz); }

Phys_space Phys_space::space;

Phys_space::Phys_space()
{
  int err = _set.insert(Phys_region(0, Phys_region::Addr(~0))).second;
  assert (err >= 0);

  for (auto const &md: L4::Kip::Mem_desc::all(l4re_kip()))
    {
      if (md.is_virtual())
	continue;

      switch (md.type())
	{
	case L4::Kip::Mem_desc::Arch:
	case L4::Kip::Mem_desc::Conventional:
	case L4::Kip::Mem_desc::Reserved:
	case L4::Kip::Mem_desc::Dedicated:
	case L4::Kip::Mem_desc::Shared:
	case L4::Kip::Mem_desc::Bootloader:
	    {
	      Phys_region re(l4_trunc_page(md.start()),
	                     l4_round_page(md.end())-1);
	      bool r = reserve(re);
	      d_printf(DBG_INFO, "  reserve phys memory space %014lx-%014lx (%s)\n",
                       md.start(), md.end(), r ? "ok" : "failed");
	      (void)r;
	    }
	  break;
	default:
	  break;
	}
    }

}

bool
Phys_space::alloc_from(Set::Iterator const &o, Phys_region const &r)
{
  if (r.contains(*o))
    {
      _set.remove(*o);
      return true;
    }

  if (o->start() >= r.start())
    {
      o->set_start(r.end() + 1);
      return true;
    }

  if (o->end() <= r.end())
    {
      o->set_end(r.start() - 1);
      return true;
    }

  Phys_region nr(r.end() + 1, o->end());
  o->set_end(r.start() - 1);
  int err = _set.insert(nr).second;
  assert (err >= 0);
  return true;
}

bool
Phys_space::reserve(Phys_region const &r)
{

  Set::Iterator n;
  bool res = false;
  do
    {
      n = _set.find(r);
      if (n == _set.end())
	return res;

      res = true;
    }
  while (alloc_from(n, r));

  return true;
}

bool
Phys_space::alloc(Phys_region const &r)
{

  Set::Iterator n;
  n = _set.find(r);
  if (n == _set.end())
    return false;

  if (n->contains(r))
    {
      reserve(r);
      return true;
    }

  return false;
}

Phys_space::Phys_region
Phys_space::alloc(Phys_region::Addr sz, Phys_region::Addr align)
{
  for (Set::Iterator i = _set.begin(); i != _set.end(); ++i)
    {
      if (i->start() > Phys_region::Addr(0) - sz)
	continue;

      Phys_region r((i->start() + align) & ~align, 0);
      r.set_end(r.start() + sz - 1);

      if (i->contains(r))
	{
	  reserve(r);
	  return r;
	}
    }

  return Phys_region();
}

void
Phys_space::dump()
{
  printf("unused physical memory space:\n");
  for (Set::Iterator i = _set.begin(); i != _set.end(); ++i)
    printf("  [%014lx-%014lx]\n", i->start(), i->end());
}
