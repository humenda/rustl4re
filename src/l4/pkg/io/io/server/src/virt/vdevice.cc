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
#include <l4/vbus/vbus_types.h>
#include <l4/re/error_helper>

namespace Vi {

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

l4vbus_resource_t
Device::get_resource_info(int index) const
{
  auto *resources = this->resources();

  if (index < 0 || (unsigned)index >= resources->size())
    L4Re::throw_error(-L4_ENOENT);

  Resource *r = resources->at(index);
  l4vbus_resource_t res;
  res.start = r->start();
  res.end = r->end();
  res.type = r->type();
  res.flags = 0;
  res.provider = r->provider_device_handle();
  res.id = r->id();
  return res;

}

l4vbus_device_t
Device::get_device_info() const
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

  for (auto i: _features)
    inf.type |= i->interface_type();

  inf.flags = 0;
  if (children())
    inf.flags |= L4VBUS_DEVICE_F_CHILDREN;

  return inf;
}

Io_irq_pin::Msi_src *
Device::find_msi_src(Msi_src_info si)
{
  for (Device *d = children(); d; d = d->next())
    if (Io_irq_pin::Msi_src *s = d->find_msi_src(si))
      return s;
  return nullptr;
}
}
