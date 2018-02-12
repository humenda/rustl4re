/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/util/item_alloc>
#include <l4/re/util/cap_alloc>
#include <l4/re/error_helper>
#include <l4/re/env>
#include <l4/re/mem_alloc>
#include <l4/re/rm>
#include <l4/re/dataspace>

#include "slab_alloc.h"

using L4::Cap;
using L4Re::Util::cap_alloc;
using L4Re::Dataspace;
using L4Re::chksys;
using L4Re::Env;
using L4Re::Rm;

class Single_page_alloc_static
{
private:
  enum { Num_pages = 1024 };
  L4Re::Util::Item_alloc<Num_pages> _page_map;
  l4_addr_t _blob;
  Cap<Dataspace> _mem;

public:
  Single_page_alloc_static()
  {
    //L4::cerr << "Hello from alloc\n";
    _mem = cap_alloc.alloc<Dataspace>();
    if (!_mem.is_valid())
      chksys(-L4_ENOMEM, "could not allocate capability");

    unsigned long size = Num_pages * L4_PAGESIZE;
    chksys(Env::env()->mem_alloc()->alloc(size, _mem),
           "backend storage data space");

    chksys(Env::env()->rm()
             ->attach(&_blob, size, Rm::Search_addr,
                      L4::Ipc::make_cap_rw(_mem)),
           "attaching backend storage data space");
  }

  void *alloc()
  {
    //L4::cerr << "PA:alloc...\n";
    long p = _page_map.alloc();
    if (p < 0)
      return 0;

    //L4::cerr << "PA: ok\n";
    return (void*)(_blob + (p * L4_PAGESIZE));
  }

  void free(void *p)
  {
    if (!p)
      return;

    l4_addr_t offset = l4_addr_t(p) - _blob;

    _mem->clear(offset, L4_PAGESIZE);
    _page_map.free(offset / L4_PAGESIZE);
  }
};
  

static Single_page_alloc_static _pa;

void *Single_page_alloc_base::_alloc() { return _pa.alloc(); }
void Single_page_alloc_base::_free(void *o) { _pa.free(o); }
