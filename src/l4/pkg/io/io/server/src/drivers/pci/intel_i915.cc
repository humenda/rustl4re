/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
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

struct Pci_intel_i915_drv : Driver
{
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG, "Found Intel i915 device\n");

    l4_uint32_t v;

    // PCI_ASLS
    d->cfg_read(0xfc, &v, Cfg_long);

    printf("Found Intel i915 GPU opregion at %x\n", v);

    unsigned flags =   Resource::Mmio_res
                     | Resource::F_prefetchable;

    // the opregion address might not be page aligned... (0xdaf68018)
    Resource *res = new Resource(flags, l4_trunc_page(v),
                                 l4_round_page(v + 0x2000) - 1);
    d->host()->add_resource(res);
    Device *p;
    for (p = d->host()->parent(); p && p->parent(); p = p->parent())
      ;

    if (p)
      p->request_child_resource(res, d->host());
    return 0;
  };
};

static Pci_intel_i915_drv _pci_intel_i915_drv;

struct Init
{
  Init()
  {
    _pci_intel_i915_drv.register_driver(0x8086, 0x0046);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0166);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0412);
    _pci_intel_i915_drv.register_driver(0x8086, 0x0416);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1612);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1912);
    _pci_intel_i915_drv.register_driver(0x8086, 0x1916);
  }
};

static Init init;

}
