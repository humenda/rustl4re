/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/mem_alloc>
#include <l4/re/dma_space>
#include <l4/sys/factory>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/cxx/exceptions>
#include <l4/re/error_helper>

#include "device.h"

#include <cstdio>
#include <cassert>

Resource *
Resource_list::find(l4_uint32_t id) const
{
  for (auto i = begin(); i != end(); ++i)
    if ((*i)->id() == id)
      return *i;

  return 0;
}

Resource *
Resource_list::find(char const *id) const
{
  return find(Resource::str_to_id(id));
}


void
Resource::dump(char const *ty, int indent) const
{
  //bool abs = true;

  //if (!valid())
  //  return;

  l4_uint64_t s, e;
#if 0
  if (abs)
    {
      s = abs_start();
      e = abs_end();
    }
  else
#endif
    {
      s = _s;
      e = _e;
    }

  static char const * const irq_trigger[]
    = { "none", // 0
        "raising edge", // 1
        "<unkn>", // 2
        "level high", // 3
        "<unkn>", // 4
        "falling edge", // 5
        "<unkn>", // 6
        "level low", // 7
        "<unkn>", // 8
        "both edges", // 9
        "<unkn>", // 10
        "<unkn>", // 11
        "<unkn>", // 12
        "<unkn>", // 13
        "<unkn>", // 14
        "<unkn>", // 15
    };

  char const *tp = prefetchable() ? "pref" : "non-pref";
  if (type() == Irq_res)
    tp = irq_trigger[(flags() / Irq_type_base) & 0xf];

  printf("%*.s%s%c [%014llx-%014llx %llx] %s (%dbit) (align=%llx flags=%lx)\n",
         indent, " ",
         ty, provided() ? '*' : ' ',
         s, e, (l4_uint64_t)size(),
         tp,
         is_64bit() ? 64 : 32, (unsigned long long)alignment(), flags());
}


void
Resource::dump(int indent) const
{
  static char const *ty[] = { "INVALID", "IRQ   ", "IOMEM ", "IOPORT",
                              "BUS   ",  "GPIO  ", "DMADOM", "" };

  dump(ty[type() % 8], indent);
}



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
  Size min_align = L4_PAGESIZE - 1;

  for (auto r: _rl)
    {
      Size a = cxx::max<Size>(r->alignment(), min_align);
      sz = (sz + a) & ~a;
      sz += r->size();
    }

  if (sz > parent->size())
    parent->size(sz);
}

bool
Resource_provider::_RS::alloc(Resource *parent, Device *pdev,
                              Resource *child, Device *cdev,
                              bool resize)
{
  Resource_list::iterator p = _rl.begin();
  Addr start = parent->start();
  Addr end;
  Size min_align = L4_PAGESIZE - 1;

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

bool
Resource_provider::_RS::adjust_children(Resource *self)
{
  Addr start = self->start();
  Size min_align = L4_PAGESIZE - 1;

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

void Mmio_data_space::alloc_ram(Size size, unsigned long alloc_flags)
{
  static L4::Cap<L4Re::Dma_space> dma_space;
  if (L4_UNLIKELY(!dma_space))
    {
      auto uf = L4Re::Env::env()->user_factory();
      auto d = L4Re::chkcap(L4Re::Util::make_unique_cap<L4Re::Dma_space>());
      L4Re::chksys(uf->create(d.get()));
      L4Re::chksys(d->associate(L4::Ipc::Cap<L4::Task>(),
                                L4Re::Dma_space::Space_attrib::Phys_space),
                   "associating DMA space for CPU physical");
      dma_space = d.release();
    }
  long ma_flags = L4Re::Mem_alloc::Continuous;

  ma_flags |= alloc_flags ? L4Re::Mem_alloc::Super_pages : 0;

  _ds_ram = L4Re::Util::make_unique_cap<L4Re::Dataspace>();
  if (!_ds_ram.is_valid())
    throw(L4::Out_of_memory(""));

  L4Re::chksys(L4Re::Env::env()->mem_alloc()->alloc(size, _ds_ram.get(),
                                                    ma_flags));

  l4_size_t ds_size = size;
  L4Re::Dma_space::Dma_addr phys_start;
  L4Re::chksys(dma_space->map(L4::Ipc::make_cap_rw(_ds_ram.get()), 0, &ds_size,
               L4Re::Dma_space::Attributes::None,
               L4Re::Dma_space::Bidirectional,
               &phys_start));
  if (size > (Size)ds_size)
    throw(L4::Out_of_memory("not really"));

  start(phys_start);

  // CHECK: this seems useless to me (alex)
  L4Re::chksys(L4Re::Env::env()->rm()
                 ->attach(&_r, ds_size,
                          L4Re::Rm::Search_addr | L4Re::Rm::Eager_map,
                          L4::Ipc::make_cap_rw(_ds_ram.get())));
}
