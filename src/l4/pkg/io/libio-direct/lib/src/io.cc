/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/io/io.h>
#include <l4/cxx/iostream>
#include <l4/crtn/initpriorities.h>
#include <l4/sigma0/sigma0.h>
#include <l4/re/namespace>
#include <l4/re/dataspace>
#include <l4/re/dma_space>
#include <l4/re/rm>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/factory>
#include <l4/sys/icu>
#include <l4/sys/irq>
#include <l4/sys/task>
#include <l4/util/splitlog2.h>
#include <l4/re/error_helper>
#include <l4/vbus/vbus_types.h>

#include <l4/cxx/hlist>

#include <l4/sys/icu>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/***********************************************************************
 * CFGs
 ***********************************************************************/

struct __internal_res {
  l4io_resource_t res;
  unsigned flags;
  union {
    struct {
      l4_cap_idx_t ram_cap;
      void *virt;
    };
  };
};

// normal descriptors
#define D_IOMEM(s, e)       { { L4IO_RESOURCE_MEM, 0, s, e, -1, 0}, __INTRES_NOT_DEFINED, { { 0, 0 } } }
#define D_PORT(s, e)        { { L4IO_RESOURCE_PORT, 0, s, e, -1, 0}, __INTRES_NOT_DEFINED, { { 0, 0 } } }
#define D_IRQ(i)            { { L4IO_RESOURCE_IRQ, 0, i, i, -1, 0}, __INTRES_NOT_DEFINED, { { 0, 0 } } }
#define D_IRQ_RANGE(s, e)   { { L4IO_RESOURCE_IRQ, 0, s, e, -1, 0}, __INTRES_NOT_DEFINED, { { 0, 0 } } }
// iomem-as-ram descriptor
#define D_RAMMEM(sz, flags) { { L4IO_RESOURCE_MEM, flags, sz, 0, -1, 0}, __INTRES_RAM , { { L4_INVALID_CAP, 0 } } }
// end marker descriptor
#define D_END()             { { 0, 0, 0, 0, 0, 0 }, 0, { { 0, 0 } } }

struct __internal_dev {
  const char *name;
  __internal_res *intres;
};

enum { __INTRES_NOT_DEFINED, __INTRES_RAM };

static __internal_res __internal_device_desc__legacy[] =
  {
      // in direct mode we have all I/O ports
      D_PORT(0, 0xffff),
      D_IOMEM(0x00000000, ~0UL),
      D_END()
  };

/* RV EB UP */
static __internal_res __internal_device_desc__sysctl[] =
  {
      D_IOMEM(0x10000000, 0x10000fff),
      D_END()
  };

static __internal_res __internal_device_desc__amba_kbd[] =
  {
      D_IOMEM(0x10006000, 0x10006fff),
      //D_IRQ(52), // EB-UP
      D_IRQ(39), // EB-MC
      D_END()
  };

static __internal_res __internal_device_desc__amba_mouse[] =
  {
      D_IOMEM(0x10007000, 0x10007fff),
      //D_IRQ(53), // EB-UP
      D_IRQ(40), // EB-MC
      D_END()
  };

static __internal_res __internal_device_desc__amba_lcd[] =
  {
      D_IOMEM(0x10020000, 0x10020fff),
      D_END()
  };

static __internal_res __internal_device_desc__amba_gpio0[] =
  {
      D_IOMEM(0x10013000, 0x10013fff),
      D_IRQ(70),
      D_END()
  };

static __internal_res __internal_device_desc__smsc911x[] =
  {
      // EB MC
      D_IOMEM(0x4e000000, 0x4e000fff),
      D_IRQ(41),
      D_END()
  };

static __internal_res __internal_device_desc__dma_area[] =
  {
      D_RAMMEM(0x200000, L4Re::Mem_alloc::Super_pages),
      D_END()
  };

struct __internal_dev _devs[] =
  {
    { "legacy",          __internal_device_desc__legacy,     },
    { "System Control",  __internal_device_desc__sysctl,     },
    { "AMBA KMI kbd",    __internal_device_desc__amba_kbd,   },
    { "AMBA KMI mou",    __internal_device_desc__amba_mouse, },
    { "AMBA PL110",      __internal_device_desc__amba_lcd,   },
    { "AMBA PL061 dev0", __internal_device_desc__amba_gpio0, },
    { "smsc911x",        __internal_device_desc__smsc911x,   },
    { "DMA area",        __internal_device_desc__dma_area,   },
    { 0, 0},
  };

#if 0
static int res_sz(l4io_resource_t *x)
{
  int i = 0;
  while (x[i].type)
    i++;
  return i;
}

static __internal_res *search_dev(const char *devname,
                                  struct __internal_dev *d)
{
  for (int i = 0; d[i].name; ++i)
    if (!strcmp(devname, d[i].name))
      return d[i].intres;

  return 0;
}
#endif

static __internal_res *search_mem_resource(unsigned long addr)
{
  for (int i = 0; _devs[i].name; ++i)
    {
      __internal_res *r = _devs[i].intres;
      while (r->res.type)
        {
          if (r->res.type == L4IO_RESOURCE_MEM
              && r->res.start <= addr && addr < r->res.end)
            return r;
          r++;
        }
    }

  return 0;
}

static int is_anon_ram(__internal_res *res)
{
  return res->flags == __INTRES_RAM;
}

static L4::Cap<L4Re::Dma_space> _dma_space;

static int alloc_anon_ram_resources()
{
  _dma_space = L4Re::Util::cap_alloc.alloc<L4Re::Dma_space>();
  if (!_dma_space)
    return -L4_ENOMEM;

  auto uf = L4Re::Env::env()->user_factory();

  if (int r = l4_error(uf->create(_dma_space)))
    return r;

  if (int r = _dma_space->associate(L4::Ipc::Cap<L4::Task>(),
                                    L4Re::Dma_space::Space_attrib::Phys_space))
    return r;

  for (int i = 0; _devs[i].name; ++i)
    {
      __internal_res *r = _devs[i].intres;
      while (r->res.type)
        {
          if (r->flags == __INTRES_RAM)
            {
              long ret;
              l4_size_t size = r->res.end; // second arg is size!

              L4::Cap<L4Re::Dataspace> d
                = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

              if (!d.is_valid())
                return -L4_ENOMEM;

              ret = L4Re::Env::env()->mem_alloc()->alloc(size, d, r->res.start | L4Re::Mem_alloc::Continuous);
              if (ret)
                {
                  L4Re::Util::cap_alloc.free(d, L4Re::This_task);
                  return ret;
                }

              // FIXME: possible truncation of DMA address
              L4Re::Dma_space::Dma_addr a;

              ret = _dma_space->map(d, 0, &size, L4Re::Dma_space::Attributes::None,
                                    L4Re::Dma_space::Bidirectional, &a);

              if (ret
                  // check, note, r->res.end is still the size...
                  || r->res.end > size)
                {
                  L4Re::Util::cap_alloc.free(d, L4Re::This_task);
                  return ret;
                }

              r->res.start = a;
              r->res.end = r->res.start + size - 1;
              r->ram_cap = d.cap();
            }
          r++;
        }
    }

  return 0;
}

typedef unsigned mysem_t;

static inline void mysem_init(mysem_t *m)
{ *m = 0; }

static inline void mysem_down(mysem_t *)
{}

static inline void mysem_up(mysem_t *)
{}

// structure of resource pairs we gave out
class Res : public cxx::H_list_item_t<Res>
{
public:
  explicit Res(enum l4io_resource_types_t t, l4_umword_t d1, l4_umword_t d2)
    : _type(t), _data1(d1), _data2(d2)
  { enqueue(); }

  ~Res() { dequeue(); }

  void * operator new(size_t size) throw() { return malloc(size); }
  void   operator delete(void *p)    { free(p); }

  l4_umword_t d1() { return _data1; }
  l4_umword_t d2() { return _data2; }

  static int  add(enum l4io_resource_types_t t, l4_umword_t d1, l4_umword_t d2);
  static Res *search(enum l4io_resource_types_t t, l4_addr_t start);
  static void del(Res *x);
  static void dump();

private:
  Res(Res const &);
  Res & operator= (Res const &);

  void enqueue();
  void dequeue();
  enum l4io_resource_types_t _type;
  l4_umword_t _data1, _data2;
};

typedef cxx::H_list_t<Res> Res_list;

struct Internal
{
  L4::Cap<L4::Kobject> _sigma0;
  L4::Cap<L4::Icu> _icu;
  Res_list _root;

  mysem_t mysem;

  Internal()
  {
    L4Re::Env const *e = L4Re::Env::env();

    _sigma0 = e->get_cap<L4::Kobject>("sigma0");
    if (!_sigma0)
      { // do not fail hard, if they do not give us 'sigma0' then they
        // have to live without it
        printf("libio: Warning: Query of 'sigma0' failed\n");
      }

    _icu = e->get_cap<L4::Icu>("icu");

    if (!_icu)
      printf("libio: Warning: Query of 'icu' failed\n");

    mysem_init(&mysem);

    alloc_anon_ram_resources();
  }

};


static Internal _internal __attribute__((init_priority(INIT_PRIO_LIBIO_INIT)));


void
Res::enqueue()
{
  mysem_down(&_internal.mysem);
  _internal._root.push_front(this);
  mysem_up(&_internal.mysem);
}

void
Res::dequeue()
{
  mysem_down(&_internal.mysem);
  _internal._root.remove(this);
  mysem_up(&_internal.mysem);
}

int
Res::add(enum l4io_resource_types_t t, l4_umword_t d1, l4_umword_t d2)
{
  Res *x = new Res(t, d1, d2);
  if (!x)
    return -L4_ENOMEM;
  return 0;
}

void
Res::del(Res *x)
{
  delete x;
}

Res *
Res::search(enum l4io_resource_types_t t, l4_addr_t d1)
{
  // FIXME: use RAII for the lock
  mysem_down(&_internal.mysem);
  for (Res_list::Iterator i = _internal._root.begin();
       i != _internal._root.end(); ++i)
    {
      if (t == i->_type && d1 == i->_data1)
        {
          mysem_up(&_internal.mysem);
          return *i;
        }
    }
  mysem_up(&_internal.mysem);
  return 0;
}

void
Res::dump()
{
  printf("Resource list dump:\n");
  for (Res_list::Iterator i = _internal._root.begin();
       i != _internal._root.end(); ++i)
    printf("Type: %d data: %lx %lx\n", i->_type, i->_data1, i->_data2);
  printf("Done.\n");
}

}

/***********************************************************************
 * IRQs
 ***********************************************************************/

long
l4io_request_irq(int irqnum, l4_cap_idx_t irqcap)
{
  if (irqnum > 10000)
    return -L4_ENOENT;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  L4::Cap<L4::Irq> _irqcap(irqcap);

  if (l4_error(L4Re::Env::env()->factory()->create(_irqcap)))
    return -L4_ENOENT;

  if (l4_error(_internal._icu->bind(irqnum, _irqcap)))
    return -L4_ENOENT;

  if (Res::add(L4IO_RESOURCE_IRQ, irqnum, irqcap))
    return -L4_ENOMEM;

  return 0;
}

long
l4io_release_irq(int irqnum, l4_cap_idx_t /*irq_cap*/)
{
  Res *res;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  res = Res::search(L4IO_RESOURCE_IRQ, irqnum);
  if (!res)
    return -L4_ENOENT;

  if (l4_error(_internal._icu->unbind(res->d1(), L4::Cap<L4::Irq>(res->d2()))))
    return -L4_ENOENT;

  l4_task_unmap(L4_BASE_TASK_CAP,
                l4_obj_fpage(res->d2(), 0, L4_FPAGE_RWX),
                L4_FP_ALL_SPACES);

  Res::del(res);
  return 0;
}









/***********************************************************************
 * I/O memory
 ***********************************************************************/

static long
__iomem_map_and_add(l4_addr_t phys, l4_addr_t virt,
                    unsigned long size, int flags)
{
  long r;

  r = l4sigma0_map_iomem(_internal._sigma0.cap(), phys, virt, size, flags);
  if (r)
    {
      if (r == -L4SIGMA0_NOTALIGNED)
        return -L4_EINVAL;
      if (r == -L4SIGMA0_IPCERROR)
        return -L4_ENODEV;
      return -L4_ENOMEM;
    }

  if (Res::add(L4IO_RESOURCE_MEM, virt, size))
    return -L4_ENOMEM; // also do: unmap + free_area

  return 0;
}

static long
handle_anonram(__internal_res *res, l4_addr_t *virt,
               unsigned long size, unsigned search_virt_address)
{
  long r;
  unsigned long flags = 0;
  int shift = (size & ~L4_SUPERPAGEMASK) ? L4_PAGESHIFT : L4_SUPERPAGESHIFT;

  if (search_virt_address)
    {
      // search for address
      if (!res->virt)
        {
          L4::Cap<L4Re::Dataspace> d(res->ram_cap);
          r = L4Re::Env::env()->rm()->attach(&res->virt, size,
	                                     flags | L4Re::Rm::Search_addr,
                                             L4::Ipc::make_cap_rw(d), 0, shift);
          if (r)
            {
              L4Re::Util::cap_alloc.free(d, L4Re::This_task);
              return r;
            }
        }

      *virt = (l4_addr_t)res->virt;

      return 0;
    }

  void *v;
  L4::Cap<L4Re::Dataspace> d(res->ram_cap);

  r = L4Re::Env::env()->rm()->attach(&v, size, flags, L4::Ipc::make_cap_rw(d), 0, shift);
  if (r)
    {
      L4Re::Util::cap_alloc.free(d, L4Re::This_task);
      return r;
    }

  res->virt = v;
  *virt = (l4_addr_t)v;

  return 0;
}

long
l4io_request_iomem(l4_addr_t phys, unsigned long size, int flags,
                   l4_addr_t *virt)
{
  long r;
  __internal_res *res;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  if (!(res = search_mem_resource(phys)))
    return -L4_ENOENT;

  if (phys + size - 1 > res->res.end)
    return -L4_ENOENT;

  *virt = 0;

  if (!is_anon_ram(res))
    {
      r = L4Re::Env::env()->rm()->reserve_area(virt, size,
                                               L4Re::Rm::Search_addr);
      if (r)
        return r;

      return __iomem_map_and_add(phys, *virt, size, flags);
    }
  // else: anonram
  return handle_anonram(res, virt, size, 1);
}

long
l4io_request_iomem_region(l4_addr_t phys, l4_addr_t virt,
                          unsigned long size, int flags)
{
  long r;
  __internal_res *res;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  if (!(res = search_mem_resource(phys)))
    return -L4_ENOENT;

  if (phys + size - 1 > res->res.end)
    return -L4_ENOENT;

  if (!is_anon_ram(res))
    {
      if (!(flags & L4IO_MEM_USE_RESERVED_AREA))
        {
          // only reserve if not already reserved, just assume the regions
          // are ok, i.e. virt:virt+size is inside the big reserved area
          r = L4Re::Env::env()->rm()->reserve_area(&virt, size);
          if (r)
            return r;
        }

      return __iomem_map_and_add(phys, virt, size, flags);
    }
  // else: anonram
  return handle_anonram(res, &virt, size, 0);
}

static long
__log2_chunk_iomem_unmap(l4_addr_t s, l4_addr_t /*e*/, int log2size)
{
  l4_task_unmap(L4_BASE_TASK_CAP, l4_fpage(s, log2size, L4_FPAGE_RWX),
                L4_FP_ALL_SPACES);
  return 0;
}

long
l4io_release_iomem(l4_addr_t virt, unsigned long size)
{
  long r;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  if ((r = l4util_splitlog2_hdl(virt, virt + size - 1,
                                __log2_chunk_iomem_unmap)))
    return r;

  // we just try to remember the region that was requested, so that
  // we can free the reserved vm area again
  Res *res = Res::search(L4IO_RESOURCE_MEM, virt);
  if (res)
    {
      if (res->d2() <= size)
        L4Re::Env::env()->rm()->free_area(virt);
      Res::del(res);
    }

  return 0;
}


long
l4io_search_iomem_region(l4_addr_t phys, l4_addr_t size,
                         l4_addr_t *rstart, l4_addr_t *rsize)
{
  (void) size;
  if (!_internal._sigma0)
    return -L4_ENOENT;

  // for the direct mode, we just have regions of superpages,
  // the real thing would have to search through the descriptors
  // lets hope this approximation is ok for now, otherwise we need to do
  // some trickier thing
  *rstart = phys & L4_SUPERPAGEMASK;
  *rsize  = L4_SUPERPAGESIZE;
  return 0;
}




/***********************************************************************
 * I/O ports
 ***********************************************************************/
#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/port_io.h>

static long __log2_chunk_port_map(l4_addr_t s, l4_addr_t /*e*/, int log2size)
{
  return l4util_ioport_map(_internal._sigma0.cap(), s, log2size);
}

static long __log2_chunk_port_unmap(l4_addr_t s, l4_addr_t /*e*/, int log2size)
{
  l4_task_unmap(L4_BASE_TASK_CAP, l4_iofpage(s, log2size), L4_FP_ALL_SPACES);
  return 0; // don't care about failed unmap, what to do anyway?
}
#endif

long
l4io_request_ioport(unsigned portnum, unsigned len)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
  long r;

  if (!_internal._sigma0)
    return -L4_ENOENT;

  if (portnum > 0xffff || portnum + len > 0x10000)
    return -L4_EINVAL;


  if ((r = l4util_splitlog2_hdl(portnum, portnum + len - 1,
                                __log2_chunk_port_map)))
    return r;

  return 0;
#else
  (void)portnum; (void)len;
  return -L4_ENODEV;
#endif
}

long
l4io_release_ioport(unsigned portnum, unsigned len)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)

  if (!_internal._sigma0)
    return -L4_ENOENT;

  l4util_splitlog2_hdl(portnum, portnum + len - 1,
                       __log2_chunk_port_unmap);

  return 0;
#else
  (void)portnum; (void)len;
  return -L4_ENODEV;
#endif
}

#if 0
l4io_resource_handle_t
l4io_lookup_device(const char *devname, l4io_device_t *desc)
{
  __internal_res *r = search_dev(devname, _devs);

  if (!r)
    return 0;

  if (desc)
    {
      strncpy(desc->name, devname, sizeof(desc->name));
      desc->name[sizeof(desc->name) - 1] = 0;
      desc->num_resources = res_sz(&r->res);
      desc->type = L4IO_DEVICE_OTHER;
    }
  return (l4io_resource_handle_t)&r->res;
}

int
l4io_lookup_resource(l4io_resource_handle_t reshandle,
                     enum l4io_resource_types_t type, int i,
                     l4io_resource_t *res)
{
  l4io_resource_t *a = (l4io_resource_t *)reshandle;
  int sz = res_sz((l4io_resource_t *)reshandle);

  for (; i < sz; ++i)
    if (type == a[i].type || type == L4IO_RESOURCE_ANY)
      {
	*res = a[i];
        return i + 1;
      }

  return -1;
}

l4_addr_t
l4io_request_resource_iomem(l4io_resource_handle_t reshandle,
                            l4_umword_t resnr,
                            l4_umword_t *next)
{
  int n;
  l4io_resource_t res;
  l4_addr_t v;

  n = l4io_lookup_resource(reshandle, L4IO_RESOURCE_MEM, resnr, &res);
  if (next)
    *next = n;

  if (n == -1)
    return 0;

  if (l4io_request_iomem(res.start, res.end - res.start + 1,
                         L4IO_MEM_NONCACHED, &v))
    return 0;

  return v;
}
#endif

void
l4io_request_all_ioports(void (*res_cb)(l4vbus_resource_t const *res))
{
  l4io_request_ioport(0, 0xffff);
  if (res_cb)
    {
      l4vbus_resource_t r;
      r.start = 0;
      r.end   = 0xffff;
      r.type  = L4VBUS_RESOURCE_PORT;
      r.flags = 0;
      res_cb(&r);
    }
}

int
l4io_has_resource(enum l4io_resource_types_t, l4_addr_t, l4_addr_t)
{
  return 1;
}
