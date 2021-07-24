/**
 * \file
 * \brief  Example of getting memory and it physical address
 */
/*
 * (c) 2020 Adam Lackorzynski <adam@l4re.org>
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/mem_alloc>
#include <l4/re/rm>
#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/re/dma_space>
#include <l4/re/protocols.h>
#include <l4/re/error_helper>
#include <l4/re/util/unique_cap>
#include <l4/sys/err.h>
#include <l4/sys/factory>
#include <cstdio>
#include <cstring>
#include <cassert>

static int allocate_mem(unsigned long size_in_bytes, unsigned long flags,
                        unsigned long phys_align, void **virt_addr,
                        L4Re::Util::Unique_cap<L4Re::Dma_space> &dmaspace,
                        L4Re::Dma_space::Dma_addr *phys_addr)
{
  int r;
  L4::Cap<L4Re::Dataspace> d;

  /* Allocate a free capability index for our data space */
  d = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if (!d.is_valid())
    return -L4_ENOMEM;

  size_in_bytes = l4_trunc_page(size_in_bytes);

  flags |= L4Re::Mem_alloc::Continuous;

  /* Allocate memory via a dataspace */
  if ((r = L4Re::Env::env()->mem_alloc()->alloc(size_in_bytes, d,
                                                flags, phys_align)))
    return r;

  /* Make the dataspace visible in our address space, uncached */
  *virt_addr = 0;
  if ((r = L4Re::Env::env()->rm()->attach(virt_addr, size_in_bytes,
                                          L4Re::Rm::F::Search_addr
                                           | L4Re::Rm::F::RW
                                           | L4Re::Rm::F::Cache_uncached,
                                          L4::Ipc::make_cap_rw(d), 0,
                                          flags & L4Re::Mem_alloc::Super_pages
                                            ? L4_SUPERPAGESHIFT : L4_PAGESHIFT)))
    return r;

  /* The the physical memory address of the allocated memory */

  if ((r = dmaspace->associate(L4::Ipc::Cap<L4::Task>(),
                               L4Re::Dma_space::Phys_space)))
    return r;

  l4_size_t ps = size_in_bytes;
  if ((r = dmaspace->map(L4::Ipc::make_cap_rw(d), 0, &ps,
                         L4Re::Dma_space::Attributes::None,
                         L4Re::Dma_space::Bidirectional, phys_addr)))
    return r;

  // The memory is L4Re::Mem_alloc::Continuous, i.e., there is only
  // one size.
  assert(ps == size_in_bytes);

  /* Done, virtual address is in virt_addr */
  return 0;
}

static int free_mem(void *virt_addr)
{
  int r;
  L4::Cap<L4Re::Dataspace> ds;

  /* Detach memory from our address space */
  if ((r = L4Re::Env::env()->rm()->detach(virt_addr, &ds)))
    return r;

  /* Release and return capability slot to allocator */
  L4Re::Util::cap_alloc.free(ds, L4Re::Env::env()->task().cap());

  /* All went ok */
  return 0;
}

int main(void)
{
  L4Re::Env const *e = L4Re::Env::env();
  void *virt;
  auto dmaspace = L4Re::chkcap(L4Re::Util::make_unique_cap<L4Re::Dma_space>());
  L4Re::Dma_space::Dma_addr phys;
  int r;

  if ((r = l4_error(e->user_factory()->create(dmaspace.get()))))
    return r;

  /* Allocate memory: 16k Bytes (usually) */
  if (allocate_mem(4 * L4_PAGESIZE, // 16 KByte aligned
                   2 + L4_PAGESHIFT, // 16 KByte aligned
                   0, &virt, dmaspace, &phys))
    {
      printf("The memory allocation failed\n");
      return 1;
    }

  printf("Allocated memory, virtual address: %p\n", virt);
  printf("Physical / DMA address: %llx\n", phys);

  /* Do something with the memory */
  memset(virt, 0x12, 4 * L4_PAGESIZE);

  /* Free memory */
  if (free_mem(virt))
    return 2;

  dmaspace->disassociate();

  printf("Freed and done. Bye.\n");

  return 0;
}
