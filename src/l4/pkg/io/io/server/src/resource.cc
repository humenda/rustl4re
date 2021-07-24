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
  static char const * const irq_trigger[] =
  {
    "none",             // 0
    "raising edge",     // 1
    "<unkn>",           // 2
    "level high",       // 3
    "<unkn>",           // 4
    "falling edge",     // 5
    "<unkn>",           // 6
    "level low",        // 7
    "<unkn>",           // 8
    "both edges",       // 9
    "<unkn>",           // 10
    "<unkn>",           // 11
    "<unkn>",           // 12
    "<unkn>",           // 13
    "<unkn>",           // 14
    "<unkn>",           // 15
  };

  static char const * const mem_type[2][2] =
  {
      {"32-bit non-pref", "64-bit non-pref"}, {"32-bit pref", "64-bit pref"}
  };

  char const *tp;
  if (type() == Mmio_res)
    tp = mem_type[prefetchable()][is_64bit()];
  else if (type() == Irq_res)
    tp = irq_trigger[(flags() / Irq_type_base) & 0xf];
  else
    tp = nullptr;

  if (flags() & F_empty)
    printf("%*.s%-6s  <EMPTY>\n", indent, " ", ty);
  else
    printf("%*.s%-6s%c [%014llx-%014llx %llx]%s%s (align=%llx flags=%lx)\n",
           indent, "", ty, provided() ? '*' : ' ', _s, _e, (l4_uint64_t)size(),
           tp ? " " : "", tp ? tp : "", (unsigned long long)alignment(), flags());
}


void
Resource::dump(int indent) const
{
  static char const *ty[] = { "INVLD", "IRQ", "IOMEM", "IOPORT",
                              "BUS",  "GPIO", "DMADOM", "" };

  dump(ty[type() % 8], indent);
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
                          L4Re::Rm::F::Search_addr | L4Re::Rm::F::Eager_map
                          | L4Re::Rm::F::RW,
                          L4::Ipc::make_cap_rw(_ds_ram.get())));
}
