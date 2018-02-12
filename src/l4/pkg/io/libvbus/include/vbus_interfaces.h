/*
 * (c) 2014 Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/types.h>

/**
 * Different sub-interfaces a vbus device may support.
 *
 * The IPC interface of vbus devices is divided into functional groups
 * of sub-interfaces. Every device must implement the generic interface
 * which provides general device information. According to the type of
 * device, additional functionality may be supported.
 *
 * The sub-interface constants are first of all used to divide the
 * function opcode space of the interface into these functional groups
 * (see L4VBUS_IFACE_SHIFT). They also make up a bitmask that specify
 * the type of the device, i.e. from the point of view of the client
 * a device is defined by the kinds of sub-interfaces it supports.
 */
enum l4vbus_iface_type_t {
  L4VBUS_INTERFACE_ICU = 0,
  L4VBUS_INTERFACE_GPIO,
  L4VBUS_INTERFACE_PCI,
  L4VBUS_INTERFACE_PCIDEV,
  L4VBUS_INTERFACE_PM,
  L4VBUS_INTERFACE_BUS,
  L4VBUS_INTERFACE_GENERIC = 0x20
};


enum {
  /**
   * Sub-interface ID shift.
   *
   * Divides the function opcode sent via IPC into a sub-interface ID
   * and the actual function opcode within the sub-interface.
   * \internal
   */
  L4VBUS_IFACE_SHIFT = 26
};

/**
 * Return the ID of the vbus sub-interface.
 * \internal
 *
 * \param opcode Full vbus function interface as delivered in the first
 *               parameter of the IPC.
 * \return Functional sub-interface the opcode belongs to,
 *         see l4vbus_iface_type_t.
 *
 * \note Function does not check if the sub-interface is actually valid.
 */
L4_INLINE unsigned l4vbus_subinterface(unsigned opcode)
{
  return opcode >> L4VBUS_IFACE_SHIFT;
}

/**
 * Return the function opcode within the sub-interface of the vbus command.
 * \internal
 *
 * \param opcode Full vbus function interface as delivered in the first
 *               parameter of the IPC.
 * \return Function opcode within the sub-interface, i.e. function opcode with
 *         the sub-interface ID removed.
 *
 * \note Function does not check if the function opcode returned is valid.
 */
L4_INLINE unsigned l4vbus_interface_opcode(unsigned opcode)
{
  return opcode & ((1 << L4VBUS_IFACE_SHIFT) - 1);
}

/**
 * Check if a vbus device supports a given sub-interface.
 *
 * \param dev_type   Device type as reported in l4vbus_device_t.
 * \param iface_type Sub-interface type to check for.
 *
 * \return True if the device supports the sub-interface.
 */
L4_INLINE int l4vbus_subinterface_supported(l4_uint32_t dev_type,
                                            l4vbus_iface_type_t iface_type)
{
  if (iface_type == L4VBUS_INTERFACE_GENERIC)
    return 1;

  return (dev_type & (1 << iface_type)) ? 1 : 0;
}
