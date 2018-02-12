/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "local.h"

#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/init.h>

/* will include $(CONTRIB)/drivers/pci/pci.h */
#include "pci.h"

DECLARE_INITVAR(dde26_pci);

#if 0
/** PCI device descriptor */
typedef struct l4dde_pci_dev {
	struct list_head      next;         /**< chain info */
	struct ddekit_pci_dev *ddekit_dev;  /**< corresponding DDEKit descriptor */
	struct pci_dev        *linux_dev;   /**< Linux descriptor */
} l4dde_pci_dev_t;
#endif


/*******************************************************************************************
 ** PCI data                                                                              **
 *******************************************************************************************/
/** List of Linux-DDEKit PCIDev mappings */
static LIST_HEAD(pcidev_mappings);

/** PCI bus */
static struct pci_bus *pci_bus = NULL;

static int l4dde26_pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val);
static int l4dde26_pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val);

/** PCI operations for our virtual PCI bus */
static struct pci_ops dde_pcibus_ops = {
	.read = l4dde26_pci_read,
	.write = l4dde26_pci_write,
};


/*******************************************************************************************
 ** Read/write PCI config space. This is simply mapped to the DDEKit functions.           **
 *******************************************************************************************/
static int l4dde26_pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val)
{
  return ddekit_pci_read(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, size, val);
}

static int l4dde26_pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val)
{
  return ddekit_pci_write(bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), where, size, val);
}

int pci_irq_enable(struct pci_dev *dev)
{
	int irq = dev->irq;
	int pin = 0;
	int ret;

	DEBUG_MSG("dev %p", dev);
	if (!dev)
		return -EINVAL;

	pin = (int)dev->pin;
	DEBUG_MSG("irq %d, pin %d, devfn %lx", dev->irq, dev->pin, dev->devfn);
	if (!pin) {
		dev_warn(&dev->dev,
		         "No interrupt pin configured for device %s\n",
		         pci_name(dev));
		return 0;
	}
	pin--;

	ret = ddekit_pci_irq_enable(dev->bus->number, PCI_SLOT(dev->devfn),
	                            PCI_FUNC(dev->devfn), pin, &irq);
	if (ret) {
		dev_warn(&dev->dev, "Interrupt enable failed for device %s (%d)\n",
		         pci_name(dev), ret);
		return -1;
	}

	dev_info(&dev->dev, "PCI INT %c -> GSI %d -> IRQ %d\n",
	         'A' + pin, irq, dev->irq);

	dev->irq = irq;
	return 0;
}

int __pci_enable_device(struct pci_dev *dev)
{
	WARN_UNIMPL;
	return 0;
}


/**
  * pci_enable_device - Initialize device before it's used by a driver.
  *
  * Initialize device before it's used by a driver. Ask low-level code
  * to enable I/O and memory. Wake up the device if it was suspended.
  * Beware, this function can fail.
  *
  * \param dev     PCI device to be initialized
  *
  */
int
pci_enable_device(struct pci_dev *dev)
{
	CHECK_INITVAR(dde26_pci);
//	WARN_UNIMPL;
	return pci_irq_enable(dev);
}


/**
 * pci_disable_device - Disable PCI device after use
 *
 * Signal to the system that the PCI device is not in use by the system
 * anymore.  This only involves disabling PCI bus-mastering, if active.
 *
 * \param dev     PCI device to be disabled
 */
void pci_disable_device(struct pci_dev *dev)
{
	CHECK_INITVAR(dde26_pci);
	WARN_UNIMPL;
}


void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
	//WARN_UNIMPL;
}

void pci_set_master(struct pci_dev *dev)
{
	CHECK_INITVAR(dde26_pci);
	WARN_UNIMPL;
}


int pci_create_sysfs_dev_files(struct pci_dev *pdev)
{
	return 0;
}

unsigned int pcibios_assign_all_busses(void)
{
  return 1;
}

void
pcibios_align_resource(void *data, struct resource *res,
			resource_size_t size, resource_size_t align)
{
	WARN_UNIMPL;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
#if 0
	int err;

	if ((err = pcibios_enable_resources(dev, mask)) < 0)
		return err;

	return pcibios_enable_irq(dev);
#endif
	return 0;
}

/*******************************************************************************************
 ** Initialization function                                                               **
 *******************************************************************************************/

/** Initialize DDELinux PCI subsystem.
 */
void __init l4dde26_init_pci(void)
{
	ddekit_pci_init();

	pci_bus = pci_create_bus(NULL, 0, &dde_pcibus_ops, NULL);
	Assert(pci_bus);

	pci_do_scan_bus(pci_bus);
	pci_walk_bus(pci_bus, pci_irq_enable, NULL);

	INITIALIZE_INITVAR(dde26_pci);
}

arch_initcall(l4dde26_init_pci);
