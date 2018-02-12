/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/capability>
#include <l4/re/dataspace>
#include <l4/re/util/region_mapping>
#include <l4/re/util/region_mapping_svr_2>
#include <l4/sys/cxx/ipc_epiface>

#include <l4/re/util/item_alloc>
#include <l4/re/util/cap_alloc>
#include <l4/re/error_helper>

#include "slab_alloc.h"
#include "debug.h"

class Region_ops;

typedef L4Re::Util::Region_handler<L4::Cap<L4Re::Dataspace>, Region_ops> Region_handler;

class Region_ops
{
public:
  typedef L4::Ipc::Snd_fpage Map_result;
  static int map(Region_handler const *h, l4_addr_t addr,
                 L4Re::Util::Region const &r, bool writable,
                 L4::Ipc::Snd_fpage *result);

  static void unmap(Region_handler const *h, l4_addr_t vaddr,
                    l4_addr_t offs, unsigned long size);

  static void free(Region_handler const *h, l4_addr_t start, unsigned long size);

  static void take(Region_handler const *h);
  static void release(Region_handler const *h);
};


class Region_map
: public L4Re::Util::Region_map<Region_handler, Slab_alloc>,
  public L4Re::Util::Rm_server<Region_map, Dbg>,
  public L4::Epiface_t<Region_map, L4Re::Rm>
{
private:
  typedef L4Re::Util::Region_map<Region_handler, Slab_alloc> Base;

public:
  typedef L4::Cap<L4Re::Dataspace> Dataspace;
  enum { Have_find = true };
  static int validate_ds(L4::Ipc_svr::Server_iface *sif,
                         L4::Ipc::Snd_fpage const & /*ds_cap*/, unsigned,
                         L4::Cap<L4Re::Dataspace> *ds)
  {
    // XXX: must check that ds is from trusted allocator!
    L4::Cap<L4Re::Dataspace> c = sif->rcv_cap<L4Re::Dataspace>(0);
    if (!c)
      return -L4_EINVAL;

    sif->realloc_rcv_cap(0);

    *ds = c;
    return L4_EOK;
  }

  static l4_umword_t find_res(L4::Cap<void> const &ds) { return ds.cap(); }

  static void global_init();

  Region_map();
  long op_io_page_fault(L4::Io_pager::Rights,
                        l4_fpage_t io_pfa, l4_umword_t pc,
                        L4::Ipc::Opt<l4_mword_t> &result,
                        L4::Ipc::Opt<L4::Ipc::Snd_fpage> &);

  virtual ~Region_map() {}

  void init();

  void debug_dump(unsigned long function) const;
};


