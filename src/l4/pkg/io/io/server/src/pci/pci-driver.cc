/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2010-2020 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *            Alexander Warg <warg@os.inf.tu-dresden.de>
 */

#include <pci-driver.h>

namespace Hw { namespace Pci {
namespace {

typedef std::map<l4_uint32_t, Driver *> Drv_list;

static Drv_list &driver_for_vendor_device()
{
  static Drv_list l;
  return l;
}

static Drv_list &driver_for_class()
{
  static Drv_list l;
  return l;
}

}

bool
Driver::register_driver_for_class(l4_uint32_t device_class)
{
  driver_for_class()[device_class] = this;
  return true;
}

bool
Driver::register_driver(l4_uint16_t vendor, l4_uint16_t device)
{
  driver_for_vendor_device()[((l4_uint32_t)device) << 16 | (l4_uint32_t)vendor] = this;
  return true;
}

Driver *
Driver::find(Dev *dev)
{
  //d_printf(DBG_DEBUG, "find(device_class = %x, vendor = %x, device = %x\n", device_class, vendor, device);

  Drv_list const &cls = driver_for_class();
  Drv_list const &vd = driver_for_vendor_device();

  // first take vendor and device IDs
  Drv_list::const_iterator r = vd.find(dev->vendor_device_ids());
  if (r != vd.end())
    return (*r).second;

  // use class IDs
  r = cls.find(dev->class_rev() >> 16);
  if (r != cls.end())
    return (*r).second;

  return 0;
}

}}
