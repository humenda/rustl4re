/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "vdevice.h"

#include <typeinfo>
#include <cstring>
#include <cstdio>

#include "device.h"
#include <l4/vbus/vdevice-ops.h>
#include <l4/vbus/vbus_pm-ops.h>
#include <l4/vbus/vbus_types.h>

namespace Vi {

Device::Dev_set Device::__devs;

void
Device::dump(int indent) const
{
  printf("%*.s%s: [%s]\n", indent, " ", name(), typeid(*this).name());
}

bool
Device::resource_allocated(Resource const *r) const
{
  if (!parent())
    return false;

  return parent()->resource_allocated(r);
}


Device *
Device::get_dev_by_id(l4vbus_device_handle_t id)
{
  if (id == 0)
    return this;
  else if (__devs.find(id) != __devs.end())
    return (Device*)id;
  else
    return 0;
}

struct Match_hid
{
  char const *hid;
  Device mutable *dev;

  int operator () (Device *d) const
  {
    char const *h = d->hid();
    if (h && strcmp(h, hid) == 0)
      {
        dev = d;
	return 1;
      }
    return 0;
  }
};

int
Device::get_by_hid(L4::Ipc::Iostream &ios)
{
  l4vbus_device_handle_t child;
  if (L4_UNLIKELY(!ios.get(child)))
    return -L4_EMSGTOOSHORT;

  int depth;
  if (L4_UNLIKELY(!ios.get(depth)))
    return -L4_EMSGTOOSHORT;

  unsigned long sz;
  char const *hid = 0;

  ios >> L4::Ipc::buf_in(hid, sz);

  if (0)
    printf("look for '%s' in %p\n", hid, this);

  if (!hid || sz <= 1)
    return -L4_EINVAL;

  iterator c;
  if (!child)
    c = begin(depth);
  else if ((c = iterator(this, get_dev_by_id(child), depth)) != end())
    ++c;

  if (c == end())
    return -L4_ENOENT;


  Match_hid mh;
  mh.hid = hid;
  mh.dev = 0;

  for (; c != end(); ++c)
    if (mh(*c) == 1)
      return dynamic_cast<Device*>(*c)->vbus_get_device(ios);

  return -L4_ENOENT;
}


int
Device::vbus_get_device(L4::Ipc::Iostream &ios)
{
  l4vbus_device_t inf;
  inf.num_resources = resources()->size();
  if (hid())
    {
      strncpy(inf.name, name(), sizeof(inf.name) - 1);
      inf.name[sizeof(inf.name) - 1] = 0;
    }
  else
    *inf.name = 0;
  inf.type = 0;
  for (Feature_list::const_iterator i = _features.begin();
       i != _features.end(); ++i)
    inf.type |= (*i)->interface_type();
  inf.flags = 0;
  if (children())
    inf.flags |= L4VBUS_DEVICE_F_CHILDREN;

  ios << l4vbus_device_handle_t(this);
  ios.put(inf);
  return L4_EOK;
}

Io_irq_pin::Msi_src *
Device::find_msi_src(Msi_src_info si)
{
  for (Device *d = children(); d; d = d->next())
    if (Io_irq_pin::Msi_src *s = d->find_msi_src(si))
      return s;

  return 0;
}

int
Device::vdevice_dispatch(l4_umword_t obj, l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  if (l4vbus_subinterface(func) == L4VBUS_INTERFACE_GENERIC)
    {
      switch (func)
	{
	case L4vbus_vdevice_hid:
	    {
	      char const *h = hid();
	      if (!h)
		return -L4_ENOSYS;

	      ios << h;
	      return L4_EOK;
	    }
	case L4vbus_vdevice_adr:
	    {
	      l4_uint32_t a = adr();
	      if (a == l4_uint32_t(~0))
		return -L4_ENOSYS;

	      ios << a;
	      return L4_EOK;
	    }
	case L4vbus_vdevice_get_by_hid:
	  return get_by_hid(ios);

	case L4vbus_vdevice_get_next:
	    {
	      l4vbus_device_handle_t child;
	      if (L4_UNLIKELY(!ios.get(child)))
		return -L4_EMSGTOOSHORT;

	      int depth;
	      if (L4_UNLIKELY(!ios.get(depth)))
		return -L4_EMSGTOOSHORT;

	      iterator c;
	      if (!child)
		c = begin(depth);
	      else
		c = ++iterator(this, get_dev_by_id(child), depth);

	      if (c == end())
		return -L4_ENODEV;

	      return dynamic_cast<Device*>(*c)->vbus_get_device(ios);
	    }

        case L4vbus_vdevice_get:
            return vbus_get_device(ios);

	case L4vbus_vdevice_get_resource:
	    {
	      int res_idx;
	      if (L4_UNLIKELY(!ios.get(res_idx)))
		return -L4_EMSGTOOSHORT;

              if (res_idx < 0 || (unsigned)res_idx >= resources()->size())
		return -L4_ENOENT;

	      Resource *r = resources()->at(res_idx);
	      l4vbus_resource_t res;
	      res.start = r->start();
	      res.end = r->end();
	      res.type = r->type();
	      res.flags = 0;
	      res.provider = r->provider_device_handle();
              res.id = r->id();
	      ios.put(res);
	      return L4_EOK;
	    }

        case L4vbus_vdevice_is_compatible:
            {
              unsigned long sz;
              char const *cid = 0;
              ios >> L4::Ipc::buf_in(cid, sz);
              if (sz == 0)
                return -L4_EMSGTOOSHORT;
              return match_cid(cxx::String(cid, strnlen(cid, sz))) ? 1 : 0;
            }

        case L4vbus_vdevice_get_hid:
          if (char const *_hid = hid())
            ios << _hid;
          else
            ios << "";

          return L4_EOK;

        case L4VBUS_PM_OP_SUSPEND:
          return pm_suspend();

        case L4VBUS_PM_OP_RESUME:
          return pm_resume();

	default: return -L4_ENOSYS;
	}
    }

  for (Feature_list::const_iterator i = _features.begin();
       i != _features.end(); ++i)
    {
      int e = (*i)->dispatch(obj, func, ios);
      if (e != -L4_ENOSYS)
	return e;
    }

  return -L4_ENOSYS;
}

}
