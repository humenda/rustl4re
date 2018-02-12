/*
 * (c) 2010-2014 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <alexander.warg@kernkonzept.com>
 *               Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "cfg.h"
#include "debug.h"
#include "pci.h"

namespace {

using namespace Hw::Pci;

struct Pci_ide_drv : Driver
{
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG, "Found IDE device\n");
    if (!Io_config::cfg->legacy_ide_resources(d->host()))
      return 1;

    unsigned io_flags =   Resource::Io_res
                        | Resource::F_size_aligned
                        | Resource::F_hierarchical;

    // IDE legacy IO interface
    if (!(d->cls_rev & 0x100))
      {
        d->host()->add_resource_rq(new Resource(io_flags, 0x1f0, 0x1f7));
        d->host()->add_resource_rq(new Resource(io_flags, 0x3f6, 0x3f6));
        d->host()->add_resource_rq(
          new Resource(Resource::Irq_res | Resource::Irq_type_raising_edge, 14, 14)
        );
      }
    if (!(d->cls_rev & 0x400))
      {
        d->host()->add_resource_rq(new Resource(io_flags, 0x170, 0x177));
        d->host()->add_resource_rq(new Resource(io_flags, 0x376, 0x376));
        d->host()->add_resource_rq(
          new Resource(Resource::Irq_res | Resource::Irq_type_raising_edge, 15, 15)
        );
      }
    return 0;
  };
};

static Pci_ide_drv _pci_ide_drv;

struct Init
{
  Init()
  {
    _pci_ide_drv.register_driver_for_class(0x0101);
  }
};

static Init init;

}
