/*
 * (c) 2008-2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>,
 *               Henning Schild <hschild@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#include <l4/io/io.h>
#include <l4/vbus/vbus.h>
#include <l4/io/types.h>
#include <l4/re/namespace>
#include <l4/re/rm>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/sys/factory>
#include <l4/sys/icu>
#include <l4/sys/irq>
#include <l4/sys/task>
#include <l4/sys/kdebug.h>
#include <l4/util/splitlog2.h>
#include <l4/crtn/initpriorities.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using L4::Cap;

namespace {

struct Internals
{
  Cap<void> _vbus;
  Cap<L4::Icu> _icu;

  Internals()
  : _vbus(Cap<void>::No_init), _icu(Cap<void>::No_init)
  {
    _vbus = L4Re::Env::env()->get_cap<void>("vbus");
    if (!_vbus)
      {
        printf("libio: Warning: Query of 'vbus' failed!\n");
        return;
      }

    l4vbus_device_handle_t handle = 0;
    int ret = l4vbus_get_device_by_hid(_vbus.cap(), 0, &handle, "L40009",
        L4VBUS_MAX_DEPTH, 0);
    if (ret)
      {
        printf("libio: Warning: Finding 'icu' in system bus failed with '%s'\n",
               l4sys_errtostr(ret));
        return;
      }

    _icu = L4Re::Util::cap_alloc.alloc<L4::Icu>();
    if (!_icu)
      {
        printf("libio: cannot allocate ICU cap\n");
        return;
      }

    ret = l4vbus_vicu_get_cap(_vbus.cap(), handle, _icu.cap());
    if (ret)
      {
        printf("libio: Warning: Getting 'icu' device failed.\n");
        L4Re::Util::cap_alloc.free(_icu);
        _icu.invalidate();
      }
  }
};

static Internals _internal __attribute__((init_priority(INIT_PRIO_LIBIO_INIT)));

static Cap<void> &vbus()
{
  return _internal._vbus;
}

static Cap<L4::Icu> &icu()
{
  return _internal._icu;
}

}

/***********************************************************************
 * IRQ
 ***********************************************************************/

long
l4io_request_irq(int irqnum, l4_cap_idx_t irq_cap)
{
  L4::Cap<L4::Irq> irq(irq_cap);
  long ret = l4_error(L4Re::Env::env()->factory()->create(irq));
  if (ret < 0) {
      printf("Create irq failed with %ld\n", ret);
      return ret;
  }

  ret = l4_error(icu()->bind(irqnum, irq));
  if (ret < 0) {
      printf("Bind irq to icu failed with %ld\n", ret);
      return ret;
  }

  return 0;
}

l4_cap_idx_t
l4io_request_icu()
{ return icu().cap(); }

long
l4io_release_irq(int irqnum, l4_cap_idx_t irq_cap)
{
  long ret = l4_error(icu()->unbind(irqnum, L4::Cap<L4::Irq>(irq_cap)));
  if (ret) {
      printf("Unbind irq %d from icu failed with %ld\n", irqnum, ret);
      return -1;
  }

  l4_task_unmap(L4_BASE_TASK_CAP,
                l4_obj_fpage(irq_cap, 0, L4_FPAGE_RWX),
                L4_FP_ALL_SPACES);

  return 0;
}

/***********************************************************************
 * I/O memory
 ***********************************************************************/

static long
__map_iomem(l4_addr_t phys, l4_addr_t* virt, unsigned long size, int flags)
{
  Cap<L4Re::Dataspace> iomem = L4::cap_cast<L4Re::Dataspace>(vbus());
  unsigned char align = L4_PAGESHIFT;
  l4_addr_t offset = phys & ~L4_PAGEMASK;

  if (!iomem.is_valid())
    return -L4_ENOENT;

  if (size >= L4_SUPERPAGESIZE)
    align = L4_SUPERPAGESHIFT;

  unsigned long rmflags = 0;

  if (flags & L4IO_MEM_EAGER_MAP)
    rmflags |= L4Re::Rm::Eager_map;

  if (*virt && (flags & L4IO_MEM_USE_RESERVED_AREA))
    rmflags |= L4Re::Rm::In_area;

  if (!*virt)
    rmflags |= L4Re::Rm::Search_addr;
  else
    {
      /* Check for reasonable caller behavior:
       * Offsets into a page should be the same. */
      if ((*virt & ~L4_PAGEMASK) != offset)
        return -L4_EINVAL;
      *virt &= L4_PAGEMASK;
    }

  long r = L4Re::Env::env()->rm()->attach(virt, size, rmflags,
                                          L4::Ipc::make_cap_rw(iomem),
                                          phys & L4_PAGEMASK, align);
  *virt += offset;
  return r;
}

long
l4io_request_iomem(l4_addr_t phys, unsigned long size,
                   int flags, l4_addr_t *virt)
{
  *virt = 0;
  return __map_iomem(phys, virt, size, flags);
}

long
l4io_request_iomem_region(l4_addr_t phys, l4_addr_t virt, unsigned long size,
                          int flags)
{
  if (!virt)
    return -L4_EADDRNOTAVAIL;
  return __map_iomem(phys, &virt, size, flags);
}

long
l4io_release_iomem(l4_addr_t virt, unsigned long size)
{
  (void)size;
  return L4Re::Env::env()->rm()->detach(virt, 0);
}

long
l4io_search_iomem_region(l4_addr_t phys, l4_addr_t size,
                         l4_addr_t *rstart, l4_addr_t *rsize)
{
  *rstart = l4_trunc_page(phys);
  *rsize  = l4_round_page(size + phys - *rstart);
  return 0;
}

/***********************************************************************
 * I/O ports
 ***********************************************************************/

long
l4io_request_ioport(unsigned portnum, unsigned len)
{
  l4vbus_resource_t res;
  res.type = L4IO_RESOURCE_PORT;
  res.start = portnum;
  res.end = portnum + len - 1;
  return l4vbus_request_resource(vbus().cap(), &res, 0);
}

long
l4io_release_ioport(unsigned portnum, unsigned len)
{
  l4vbus_resource_t res;
  res.type = L4IO_RESOURCE_PORT;
  res.start = portnum;
  res.end = portnum + len - 1;
  return l4vbus_release_resource(vbus().cap(), &res);
}

/***********************************************************************
 * General
 ***********************************************************************/

int
l4io_iterate_devices(l4io_device_handle_t *devhandle,
                     l4io_device_t *dev, l4io_resource_handle_t *reshandle)
{
  if (!vbus().is_valid())
    return -L4_ENOENT;

  if (reshandle)
    *reshandle = 0;

  return l4vbus_get_next_device(vbus().cap(), L4VBUS_NULL,
                                devhandle, L4VBUS_MAX_DEPTH, dev);
}

int
l4io_lookup_device(const char *devname,
                   l4io_device_handle_t *dev_handle, l4io_device_t *dev,
                   l4io_resource_handle_t *res_handle)
{
  int r;
  l4io_device_handle_t dh = 0;

  if (!vbus().is_valid())
    return -L4_ENOENT;

  if ((r = l4vbus_get_device_by_hid(vbus().cap(), 0,
                                    &dh, devname, L4VBUS_MAX_DEPTH, dev)))
    return r;

  if (dev_handle)
    *dev_handle = dh;

  if (res_handle)
    *res_handle = 0;

  return -L4_EOK;
}

int
l4io_lookup_resource(l4io_device_handle_t devhandle,
                     enum l4io_resource_types_t type,
                     l4io_resource_handle_t *res_handle,
                     l4io_resource_t *desc)
{
  l4vbus_resource_t resource;
  while (!l4vbus_get_resource(vbus().cap(), devhandle, *res_handle, &resource))
    {
      ++(*res_handle);
      // copy device description
      if (resource.type == type || type == L4IO_RESOURCE_ANY)
	{
	  *desc = resource;
	  return -L4_EOK;
	}
    }

  return -L4_ENOENT;
}

l4_addr_t
l4io_request_resource_iomem(l4io_device_handle_t devhandle,
                            l4io_resource_handle_t *reshandle)
{
  l4io_resource_t res;
  l4_addr_t v;

  if (l4io_lookup_resource(devhandle, L4IO_RESOURCE_MEM,
                           reshandle, &res))
    return 0;

  v = 0;
  if (l4io_request_iomem(res.start, res.end - res.start + 1,
                         L4IO_MEM_NONCACHED, &v))
    return 0;

  return v;
}

void
l4io_request_all_ioports(void (*res_cb)(l4vbus_resource_t const *res))
{
  l4vbus_device_handle_t next_dev = 0;
  l4vbus_device_t info;

  while (!l4vbus_get_next_device(vbus().cap(), l4io_get_root_device(),
                                 &next_dev, L4VBUS_MAX_DEPTH, &info))
    {
      l4vbus_resource_t resource;
      for (unsigned r = 0; r < info.num_resources; ++r)
	{
	  l4vbus_get_resource(vbus().cap(), next_dev, r, &resource);
	  if (resource.type == L4IO_RESOURCE_PORT)
            {
	      l4vbus_request_resource(vbus().cap(), &resource, 0);
              if (res_cb)
                res_cb(&resource);
            }
	}
    }
}

int
l4io_has_resource(enum l4io_resource_types_t type,
                  l4vbus_paddr_t start, l4vbus_paddr_t end)
{
  l4io_device_handle_t dh = l4io_get_root_device();
  l4io_device_t dev;
  l4io_resource_handle_t reshandle;

  while (1)
    {
      l4io_resource_t res;

      if (l4io_iterate_devices(&dh, &dev, &reshandle))
        break;

      if (dev.num_resources)
        while (!l4io_lookup_resource(dh, type, &reshandle, &res))
          if (start >= res.start && end <= res.end)
            return 1;
    }
  return 0;
}
