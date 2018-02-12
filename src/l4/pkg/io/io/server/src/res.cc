/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/capability>
#include <l4/sys/kip>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>

#include <l4/cxx/avl_tree>
#include <l4/cxx/bitmap>
#include <l4/sys/ipc.h>
#include <l4/sigma0/sigma0.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "debug.h"
#include "res.h"
#include "cfg.h"

enum
{
  Page_shift   = L4_PAGESHIFT,
  Min_rs_bits = 10,
  Min_rs_mul  = 1UL << Min_rs_bits,
  Min_rs      = Page_shift + Min_rs_bits,
};


struct Phys_region
{
  l4_addr_t phys;
  l4_addr_t size;

  bool operator < (Phys_region const &o) const throw()
  { return phys + size - 1 < o.phys; }
  bool contains(Phys_region const &o) const throw()
  { return o.phys >= phys && o.phys + o.size -1 <= phys + size -1; }
  Phys_region() = default;
  Phys_region(l4_addr_t phys, l4_addr_t size) : phys(phys), size(size) {}
};

struct Io_region : public Phys_region, public cxx::Avl_tree_node
{
  l4_addr_t virt;

  mutable cxx::Bitmap_base pages;

  Io_region() : pages(0) {}
  Io_region(Phys_region const &pr) : Phys_region(pr), virt(0), pages(0) {}
};

struct Io_region_get_key
{
  typedef Phys_region Key_type;
  static Phys_region const &key_of(Io_region const *o) { return *o; }

};

typedef cxx::Avl_tree<Io_region, Io_region_get_key> Io_set;

static Io_set io_set;

static L4::Cap<void> sigma0;

int res_init()
{
  using L4Re::chkcap;
  sigma0 = chkcap(L4Re::Env::env()->get_cap<void>("sigma0"), "getting sigma0 cap", 0);

#if 0
  L4::Kip::Mem_desc *md = L4::Kip::Mem_desc::first(l4re_kip());
  L4::Kip::Mem_desc *mde = md + L4::Kip::Mem_desc::count(l4re_kip());
  for (; md < mde; ++md)
    {
      if (md->type() == L4::Kip::Mem_desc::Arch)
	{
	  printf("ARCH REGION %014lx-%014lx (%02x:%02x)\n",
	         md->start(), md->end(), md->type(), md->sub_type());
	  res_map_iomem(md->start(), md->size());
	}
    }
#endif
  return 0;
};

static long
map_iomem_range(l4_addr_t phys, l4_addr_t virt, l4_addr_t size)
{
  unsigned p2sz = L4_PAGESHIFT;
  long res;

  if ((phys & ~(~0UL << p2sz)) || (virt & ~(~0UL << p2sz))
      || (size & ~(~0UL << p2sz)))
    return -L4_ERANGE;

  while (size)
    {
#if 1
      while (p2sz < 22)
	{
	  unsigned n = p2sz + 1;
	  if ((phys & ~(~0UL << n)) || ((1UL << n) > size))
	    break;
	  ++p2sz;
	}
#endif
      l4_msg_regs_t *m = l4_utcb_mr_u(l4_utcb());
      l4_buf_regs_t *b = l4_utcb_br_u(l4_utcb());
      l4_msgtag_t tag = l4_msgtag(L4_PROTO_SIGMA0, 2, 0, 0);
      m->mr[0] = SIGMA0_REQ_FPAGE_IOMEM;
      m->mr[1] = l4_fpage(phys, p2sz, L4_FPAGE_RWX).raw;

      b->bdr   = 0;
      b->br[0] = L4_ITEM_MAP;
      b->br[1] = l4_fpage(virt, p2sz, L4_FPAGE_RWX).raw;
      tag = l4_ipc_call(sigma0.cap(), l4_utcb(), tag, L4_IPC_NEVER);

      res = l4_error(tag);
      if (res < 0)
	return res;

      phys += 1UL << p2sz;
      virt += 1UL << p2sz;
      size -= 1UL << p2sz;

      if (!size)
	break;

      while ((1UL << p2sz) > size)
	--p2sz;
    }
  return 0;
}

l4_addr_t res_map_iomem(l4_addr_t phys, l4_addr_t size)
{
  int p2size = Min_rs;
  while ((1UL << p2size) < (size + (phys - l4_trunc_size(phys, p2size))))
    ++p2size;

  Io_region *iomem = 0;
  Phys_region r;
  r.phys = l4_trunc_page(phys);
  r.size = l4_round_page(size + phys - r.phys);

  // we need to look for proper aligned and sized regions here
  Phys_region io_reg(l4_trunc_size(phys, p2size), 1UL << p2size);

  while (1)
    {
      Io_region *reg = io_set.find_node(io_reg);
      if (!reg)
	{
	  iomem = new Io_region(io_reg);

          // start searching for virtual region at L4_PAGESIZE
	  iomem->virt = L4_PAGESIZE;
	  int res = L4Re::Env::env()->rm()->reserve_area(&iomem->virt,
	      iomem->size, L4Re::Rm::Search_addr, p2size);

	  if (res < 0)
	    {
	      delete iomem;
	      return 0;
	    }

	  unsigned bytes
	    = cxx::Bitmap_base::bit_buffer_bytes(iomem->size >> Page_shift);
	  iomem->pages = cxx::Bitmap_base(malloc(bytes));
	  memset(iomem->pages.bit_buffer(), 0, bytes);

	  io_set.insert(iomem);

	  d_printf(DBG_DEBUG, "new iomem region: p=%lx v=%lx s=%lx (bmb=%p)\n",
                   iomem->phys, iomem->virt, iomem->size,
                   iomem->pages.bit_buffer());
	  break;
	}
      else if (reg->contains(r))
	{
	  iomem = reg;
	  break;
	}
      else
	{
	  if (reg->pages.bit_buffer())
	    free(reg->pages.bit_buffer());

	  io_set.remove(*reg);
	  delete reg;
	}
    }

  l4_addr_t min = 0, max;
  bool not_mapped = false;
  int all_ok = 0;

  for (unsigned i = (r.phys - iomem->phys) >> Page_shift;
       i <= ((r.size + r.phys - iomem->phys) >> Page_shift);
       ++i)
    {
      if (not_mapped && (i == ((r.size + r.phys - iomem->phys) >> Page_shift)
	  || iomem->pages[i]))
	{
	  max = i << Page_shift;
	  not_mapped = false;

	  int res = map_iomem_range(iomem->phys + min, iomem->virt + min,
	                            max - min);

	  d_printf(DBG_DEBUG2, "map mem: p=%lx v=%lx s=%lx: %s(%d)\n",
	           iomem->phys + min,
                   iomem->virt + min, max - min,
                   res < 0 ? "failed" : "done", res);

	  if (res >= 0)
	    {
	      for (unsigned x = min >> Page_shift; x < i; ++x)
		iomem->pages.set_bit(x);
	    }
	  else
	    all_ok = res;
	}
      else if (!not_mapped && !iomem->pages[i])
	{
	  min = i << Page_shift;
	  not_mapped = true;
	}
    }

  if (all_ok < 0)
    return 0;

  return iomem->virt + phys - iomem->phys;
}

#if defined(ARCH_amd64) || defined(ARCH_x86)

#include <l4/util/port_io.h>

static l4_umword_t iobitmap[0x10000 / L4_MWORD_BITS];

static l4_umword_t get_iobit(unsigned port)
{
  return iobitmap[port / L4_MWORD_BITS] & (1UL << (port % L4_MWORD_BITS));
}

static void set_iobit(unsigned port)
{
  iobitmap[port / L4_MWORD_BITS] |= (1UL << (port % L4_MWORD_BITS));
}

int res_get_ioport(unsigned port, int size)
{
  bool map = false;
  for (unsigned i = 0; i < (1UL << size); ++i)
    if (!get_iobit(port + i))
      {
	map = true;
	break;
      }

  if (!map)
    return 0;

  int res = l4util_ioport_map(sigma0.cap(), port, size);
  if (res == 0)
    {
      for (unsigned i = 0; i < (1UL << size); ++i)
	set_iobit(port + i);
      return 0;
    }

  return res;
}

#endif
