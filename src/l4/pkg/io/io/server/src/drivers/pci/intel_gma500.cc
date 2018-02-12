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

struct Pci_intel_gma500_drv : Driver
{
  int probe(Dev *d)
  {
    d_printf(DBG_DEBUG, "Found Intel gma500 device\n");

    l4_uint32_t v;

    // GC - Graphics Control
    d->cfg_read(0x52, &v, Cfg_short);

    unsigned gfx_mem_sz = 0;

    // VGA disabled?
    if (v & 2)
      return 1;

    switch ((v >> 4) & 7)
      {
        case 1: gfx_mem_sz = 1 << 20; break;
        case 2: gfx_mem_sz = 4 << 20; break;
        case 3: gfx_mem_sz = 8 << 20; break;
        default: return 1;
      }

    // BSM - Base of Stolen Memory
    d->cfg_read(0x5c, &v, Cfg_long);
    v &= 0xfff00000;

    unsigned flags =   Resource::Mmio_res
                     | Resource::F_prefetchable;
    l4_addr_t end = v + gfx_mem_sz - 1;

    Resource *res = new Resource(flags, v, end);
    d->host()->add_resource(res);
    Device *p;
    for (p = d->host()->parent(); p && p->parent(); p = p->parent())
      ;

    if (p)
      p->request_child_resource(res, d->host());
    return 0;
  }
};

static Pci_intel_gma500_drv _pci_intel_gma500_drv;

struct Init
{
  Init()
  {
    _pci_intel_gma500_drv.register_driver(0x8086, 0x8108);
  }
};

static Init init;

}
