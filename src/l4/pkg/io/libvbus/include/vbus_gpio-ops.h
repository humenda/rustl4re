/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/vbus/vbus_interfaces.h>

enum L4vbus_gpio_op
{
  L4VBUS_GPIO_OP_SETUP = L4VBUS_INTERFACE_GPIO << L4VBUS_IFACE_SHIFT,
  L4VBUS_GPIO_OP_CONFIG_PAD,
  L4VBUS_GPIO_OP_CONFIG_GET,
  L4VBUS_GPIO_OP_GET,
  L4VBUS_GPIO_OP_SET,
  L4VBUS_GPIO_OP_MULTI_SETUP,
  L4VBUS_GPIO_OP_MULTI_CONFIG_PAD,
  L4VBUS_GPIO_OP_MULTI_GET,
  L4VBUS_GPIO_OP_MULTI_SET,
  L4VBUS_GPIO_OP_TO_IRQ,
  L4VBUS_GPIO_OP_CONFIG_PULL
};
