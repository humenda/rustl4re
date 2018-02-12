/*
 * (c) 2014 Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/vbus/vbus_interfaces.h>

enum
{
  L4vbus_pciroot_cfg_read = L4VBUS_INTERFACE_PCI << L4VBUS_IFACE_SHIFT,
  L4vbus_pciroot_cfg_write,
  L4vbus_pciroot_cfg_irq_enable
};

enum
{
  L4vbus_pcidev_cfg_read = L4VBUS_INTERFACE_PCIDEV << L4VBUS_IFACE_SHIFT,
  L4vbus_pcidev_cfg_write,
  L4vbus_pcidev_cfg_irq_enable
};
