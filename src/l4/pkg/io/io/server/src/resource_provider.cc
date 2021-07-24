// SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
/*
 * Copyright (C) 2010 Technische Universität Dresden (Germany)
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 * Copyright (C) 2019 Kernkonzept GmbH.
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/mem_alloc>
#include <l4/re/dma_space>
#include <l4/sys/factory>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/cxx/exceptions>
#include <l4/re/error_helper>

#include "device.h"
#include "resource_provider.h"

#include <cstdio>
#include <cassert>


bool
Resource_provider::_RS::request(Resource *parent, Device *,
                                Resource *child, Device *)
{
  Addr start = child->start();
  Addr end   = child->end();

  if (end < start)
    return false;

  if (start < parent->start())
    return false;

  if (end > parent->end())
    return false;

  Resource_list::iterator r = _rl.begin();
  while (true)
    {
      if (r == _rl.end() || (*r)->start() > end)
       {
         // insert before r
         _rl.insert(r, child);
         child->parent(parent);
         return true;
       }

      if ((*r)->end() >= start)
       return false;

      ++r;
    }
}

void
Resource_provider::_RS::assign(Resource *parent, Resource *child)
{
  auto p = _rl.begin();
  for ( ; p != _rl.end(); ++p)
    {
      if ((*p)->alignment() <= child->alignment())
        break;
    }

  _rl.insert(p, child);
  child->parent(parent);

  auto f = _rl.front();
  if (f->alignment() > parent->alignment())
    parent->alignment(f->alignment());

  Size sz = 0;
  Size min_align = this->min_align(parent);

  for (auto r: _rl)
    {
      Size a = cxx::max<Size>(r->alignment(), min_align);
      sz = (sz + a) & ~a;
      sz += r->size();
    }

  if (sz > parent->size())
    parent->size(sz);
}

/**
 * Allocating child resources from the resource list '_rl'.
 *
 * Resource alignment is enforced.
*/
bool
Resource_provider::_RS::alloc(Resource *parent, Device *pdev,
                              Resource *child, Device *cdev,
                              bool resize)
{
  Resource_list::iterator p = _rl.begin();
  Addr start = parent->start();
  Addr end;
  Size min_align = this->min_align(parent);

  if (p != _rl.end() && (*p)->start() == start)
    {
      start = (*p)->end() + 1;
      ++p;
    }

  while (true)
    {
      if (p != _rl.end())
       end = (*p)->start() - 1;
      else
       end = parent->end();

      Size align = cxx::max<Size>(child->alignment(), min_align);
      start = (start + align) & ~align; // pad to get alignment

      if (start < end && end - start >= (Addr)child->size() - 1)
       {
         child->start(start);
         break;
       }

      if (p == _rl.end() && !resize)
       return false;

      if (p == _rl.end() && resize)
       {
         end = start + child->size() - 1;
         if (end < start)
           return false; // wrapped around

         parent->end(end);
         child->start(start);
         break;
       }

      start = (*p)->end() + 1;
      ++p;
    }

  if (child->provided())
    child->provided()->adjust_children(child);

  return request(parent, pdev, child, cdev);
}

/**
 * Relocate child resources according to the resource list '_rl'.
 *
 * Enforce the same alignment as in alloc() and assign().
 */
bool
Resource_provider::_RS::adjust_children(Resource *self)
{
  Addr start = self->start();
  Size min_align = this->min_align(self);

  for (auto c: _rl)
    {
      if (c->fixed_addr() || c->relative() || c->empty())
        {
          d_printf(DBG_WARN,
                   "internal warning: skipped unallocated fixed / relative resource\n");
          continue;
        }

      Size a = cxx::max<Size>(c->alignment(), min_align);
      start = (start + a) & ~a;
      c->start(start);

      start += c->size();

      if (!self->contains(*c))
        {
          d_printf(DBG_ERR, "error: resource setting failed: ");
          c->dump();
          d_printf(DBG_ERR, "  in ");
          self->dump();
        }

      c->enable();

      if (c->provided())
        c->provided()->adjust_children(c);
    }
  return true;
}
