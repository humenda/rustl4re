/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "vproxy_dev.h"
#include "vbus_factory.h"

namespace Vi {

Proxy_dev::Proxy_dev(Hw::Device *d)
: _hwd(d)
{
  d->add_client(this);
  // suck features from real dev
  for (Hw::Device::Feature_list::const_iterator i = d->features()->begin();
       i != d->features()->end(); ++i)
    {
      Dev_feature *vf = Feature_factory::match(*i);
      if (vf)
	add_feature(vf);
    }

  // suck resources from our real dev
  for (Resource_list::const_iterator i = d->resources()->begin();
       i != d->resources()->end(); ++i)
    {
      if (!*i)
        continue;

      if ((*i)->disabled())
	continue;

      Resource *vr = Resource_factory::match(*i);
      if (!vr)
	vr = *i;

      add_resource(vr);
    }
}

namespace {
  static Dev_factory_t<Proxy_dev, Hw::Device> __ghwdf;
}

}
