/*!
 * \file	pci.h
 * \brief	PCI stuff
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef __PCI_H_
#define __PCI_H_

#include <l4/io/io.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_pci.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include "iomem.h"
#include "init.h"

#define __init	__attribute__((section(".text.init")))

struct pci_device_id
{
  unsigned short vendor, device;
  unsigned short svid, sid;
  unsigned long driver_data;
};

int  pci_probe(con_accel_t *accel);
void pci_register(const struct pci_device_id *tbl, 
		  int(*probe)(unsigned int bus, unsigned int devfn,
		              const struct pci_device_id *dev, 
			      con_accel_t *accel));
void pci_resource(unsigned int bus, unsigned int devfn, int num, 
		  l4_addr_t *addr, l4_size_t *size);

extern l4_cap_idx_t vbus;
extern l4vbus_device_handle_t root_bridge;

/* krishna: looks a bit sick, but we want complete L4IO compatibility NOW.

   1. Do not use pcibios_*() or pci_*() from pcilib directly - use these macros.
   2. Test for con_hw_not_use_l4io before real execution

   l4io_pdev_t handle is stored in [bus:devfn].
*/

#define PCIBIOS_READ_CONFIG_BYTE(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_read(vbus, root_bridge, bus, devfn, where, (l4_uint32_t*)val, 8);	\
	} while (0)

#define PCIBIOS_READ_CONFIG_WORD(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_read(vbus, root_bridge, bus, devfn, where, (l4_uint32_t*)val, 16);	\
	} while (0)

#define PCIBIOS_READ_CONFIG_DWORD(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_read(vbus, root_bridge, bus, devfn, where, (l4_uint32_t*)val, 32);	\
	} while (0)

#define PCIBIOS_WRITE_CONFIG_BYTE(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_write(vbus, root_bridge, bus, devfn, where, (l4_uint32_t)val, 8);	\
	} while (0)

#define PCIBIOS_WRITE_CONFIG_WORD(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_write(vbus, root_bridge, bus, devfn, where, (l4_uint32_t)val, 16);	\
	} while (0)

#define PCIBIOS_WRITE_CONFIG_DWORD(bus, devfn, where, val)	\
	do {							\
	    l4vbus_pci_cfg_write(vbus, root_bridge, bus, devfn, where, (l4_uint32_t)val, 32);	\
	} while (0)

#endif
