/*!
 * \file	pci.c
 * \brief	Handling of PCI devices
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

#include <stdio.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/err.h>

#include "pci.h"
#include "init.h"

#define MAX_DRIVERS 16

struct pci_driver
{
  const struct pci_device_id *dev;
  int (*probe)(unsigned int bus, unsigned int devfn,
	       const struct pci_device_id *dev, con_accel_t *accel);
};

static struct pci_driver pci_drv[MAX_DRIVERS];

void
pci_register(const struct pci_device_id *tbl, 
	     int(*probe)(unsigned int bus, unsigned int devfn,
			 const struct pci_device_id *dev, con_accel_t *accel))
{
  struct pci_driver *drv;

  for (drv = pci_drv; drv->dev; drv++)
    {
      if (drv >= pci_drv + MAX_DRIVERS-1)
	{
	  printf("Too many drivers registered, increase MAX_DRIVERS!!\n");
	  return;
	}
    }

  drv->dev   = tbl;
  drv->probe = probe;
}

l4_cap_idx_t vbus = L4_INVALID_CAP;
l4vbus_device_handle_t root_bridge = 0;

int
pci_probe(con_accel_t *accel)
{
  long err;
  int ret;

  vbus = l4re_env_get_cap("vbus");
  if (l4_is_invalid_cap(vbus))
    {
      printf("PCI: query vbus service failed, no PCI\n");
      return -L4_ENODEV;
    }

  err = l4vbus_get_device_by_hid(vbus, 0, &root_bridge, "PNP0A03", L4VBUS_MAX_DEPTH, 0);

  if (err < 0) {
      printf("PCI: no root bridge found, no PCI\n");
      return -L4_ENODEV;
  }

  unsigned bus = 0;
  unsigned slot, fn = 0;
  /* only scan possible devices and subdevices on the first bus */
  for (slot = 0; slot < 32; slot++)
    //for (fn = 0; fn < 8; fn ++)
      {
	unsigned devfn = (slot << 16) | fn;
	struct pci_driver *drv;
	const struct pci_device_id *dev;

	/* only scan for graphics cards */
	unsigned class_id = 0;
	PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_CLASS_DEVICE, &class_id);
	if (class_id != PCI_CLASS_DISPLAY_VGA)
	  continue;
	printf("Found VGA device\n");

	for (drv = pci_drv; drv->dev; drv++)
	  {
	    for (dev = drv->dev; dev->vendor; dev++)
	      {
		unsigned vendor = 0, device = 0, sub_vendor = 0, sub_device = 0;
		PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_VENDOR_ID, &vendor);
		PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_DEVICE_ID, &device);
		PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_SUBSYSTEM_VENDOR_ID, &sub_vendor);
		PCIBIOS_READ_CONFIG_WORD(bus, devfn, PCI_SUBSYSTEM_ID, &sub_device);
		if (dev->vendor != vendor)
		  continue;
		if (dev->device != 0)
		  if (dev->device != device)
		    continue;
		if (dev->svid != 0)
		  if ((dev->svid != sub_vendor) ||
		      (dev->sid != sub_device))
		    continue;

		/* found appropriate driver ... */
		if ((ret = drv->probe(bus, devfn, dev, accel)) != 0)
		  /* ... YES */
		  continue;

		return 0;
	      }
	  }
      }

  return -L4_ENODEV;
}

void
pci_resource(unsigned int bus, unsigned int devfn, 
	     int num, l4_addr_t *addr, l4_size_t *size)
{
  unsigned l, sz, reg;

  switch (num)
    {
    case 0:  reg = PCI_BASE_ADDRESS_0; break;
    case 1:  reg = PCI_BASE_ADDRESS_1; break;
    case 2:  reg = PCI_BASE_ADDRESS_2; break;
    default: return;
    }

  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, reg, &l);
  PCIBIOS_WRITE_CONFIG_DWORD(bus, devfn, reg, ~0);
  PCIBIOS_READ_CONFIG_DWORD (bus, devfn, reg, &sz);
  PCIBIOS_WRITE_CONFIG_DWORD(bus, devfn, reg, l);
  if ((l & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY)
    {
      *addr = l & PCI_BASE_ADDRESS_MEM_MASK;
      sz   &= PCI_BASE_ADDRESS_MEM_MASK;
      *size = sz & ~(sz - 1);
    }
  else
    {
      *addr = l & PCI_BASE_ADDRESS_IO_MASK;
      sz   &= PCI_BASE_ADDRESS_IO_MASK & 0xffff;
      *size = sz & ~(sz - 1);
    }
}
