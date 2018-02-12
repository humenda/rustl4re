/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/vbus/vbus_types.h>
#include <l4/sys/types.h>

/**
 * \ingroup l4vbus_module
 * \defgroup l4vbus_pci_module L4vbus PCI functions
 * \{
 */


__BEGIN_DECLS

/**
 * \copybrief L4vbus::Pci_host_bridge::cfg_read()
 * \param vbus   Capability of the system bus
 * \param handle Device handle of the PCI root bridge
 * \copydetails L4vbus::Pci_host_bridge::cfg_read()
 */
int L4_CV
l4vbus_pci_cfg_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                    l4_uint32_t bus, l4_uint32_t devfn,
                    l4_uint32_t reg, l4_uint32_t *value, l4_uint32_t width);

/**
 * \copybrief L4vbus::Pci_host_bridge::cfg_write()
 * \param vbus   Capability of the system bus
 * \param handle Device handle of the PCI root bridge
 * \copydetails L4vbus::Pci_host_bridge::cfg_write()
 */
int L4_CV
l4vbus_pci_cfg_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                     l4_uint32_t bus, l4_uint32_t devfn,
                     l4_uint32_t reg, l4_uint32_t value, l4_uint32_t width);

/**
 * \copybrief L4vbus::Pci_host_bridge::irq_enable()
 * \param vbus   Capability of the system bus
 * \param handle Device handle of the PCI root bridge
 * \copydetails L4vbus::Pci_host_bridge::irq_enable()
 */
int L4_CV
l4vbus_pci_irq_enable(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      l4_uint32_t bus, l4_uint32_t devfn,
                      int pin, unsigned char *trigger,
                      unsigned char *polarity);


/**
 * \copybrief L4vbus::Pci_dev::cfg_read()
 * \param vbus   Capability of the system bus
 * \param handle Device handle of the PCI device
 * \copydetails L4vbus::Pci_dev::cfg_read()
 */
int L4_CV
l4vbus_pcidev_cfg_read(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                       l4_uint32_t reg, l4_uint32_t *value, l4_uint32_t width);

/**
 * \copybrief L4vbus::Pci_dev::cfg_write()
 * \param  vbus         Capability of the system bus
 * \param  handle       Device handle of the PCI device
 * \copydetails L4vbus::Pci_dev::cfg_write()
 */
int L4_CV
l4vbus_pcidev_cfg_write(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                        l4_uint32_t reg, l4_uint32_t value, l4_uint32_t width);

/**
 * \copybrief L4vbus::Pci_dev::irq_enable()
 * \param  vbus         Capability of the system bus
 * \param  handle       Device handle of the PCI device
 * \copydetails L4vbus::Pci_dev::irq_enable()
 */
int L4_CV
l4vbus_pcidev_irq_enable(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                         unsigned char *trigger,
                         unsigned char *polarity);



/**\}*/
__END_DECLS
