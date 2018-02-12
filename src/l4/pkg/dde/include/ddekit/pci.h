/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

#include <l4/dde/ddekit/types.h>

/** \defgroup DDEKit_pci */

/** Our version of PCI_ANY_ID */
#define DDEKIT_PCI_ANY_ID    (unsigned short)(~0)

/** Copy of L4IO_PCIDEV_RES */
#define DDEKIT_PCIDEV_RES	 12

struct ddekit_pci_dev;

/** PCI resource descriptor. Copied from generic_io. 
 *
 * XXX!
 */
typedef struct ddekit_pci_resource {
	unsigned long start;
	unsigned long end;
	unsigned long flags;
} ddekit_pci_res_t;

void ddekit_pci_init(void);

int ddekit_pci_get_device(int nr, int *bus, int *slot, int *func);

int ddekit_pci_read(int bus, int slot, int func, int pos, int len, ddekit_uint32_t *val);
int ddekit_pci_write(int bus, int slot, int func, int pos, int len, ddekit_uint32_t val);

int ddekit_pci_irq_enable(int bus, int slot, int func, int pin, int *irq);

/** Read byte from PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     read value
 *
 * \return 0       success
 */
int ddekit_pci_readb (int bus, int slot, int func, int pos, ddekit_uint8_t  *val);

/** Read word from PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     read value
 *
 * \return 0       success
 */
int ddekit_pci_readw (int bus, int slot, int func, int pos, ddekit_uint16_t *val);

/** Read dword from PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     read value
 *
 * \return 0       success
 */
int ddekit_pci_readl (int bus, int slot, int func, int pos, ddekit_uint32_t *val);

/** Write byte to PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     value to write
 *
 * \return 0       success
 */
int ddekit_pci_writeb(int bus, int slot, int func, int pos, ddekit_uint8_t   val);

/** Write word to PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     value to write
 *
 * \return 0       success
 */
int ddekit_pci_writew(int bus, int slot, int func, int pos, ddekit_uint16_t  val);

/** Write word to PCI config space.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus      bus ID
 * \param slot     slot #
 * \param func     function #
 * \param pos      offset in config space
 * \retval val     value to write
 *
 * \return 0       success
 */
int ddekit_pci_writel(int bus, int slot, int func, int pos, ddekit_uint32_t  val);

/** Find a PCI device.
 *
 * \ingroup DDEKit_pci
 *
 * \param bus	pointer to bus number or \ref DDEKIT_PCI_ANY_ID
 * \param slot  pointer to slot number (devfn >> DEVFN_SLOTSHIFT) or \ref DDEKIT_PCI_ANY_ID
 * \param func  pointer to func number (devfc & DEVFN_FUNCMASK) or \ref DDEKIT_PCI_ANY_ID
 * \param start search device list only behind this device (excluding it!), NULL
 *              searches whole device list
 *
 * \retval bus      bus number
 * \retval slot     slot number
 * \retval func     function number
 *
 * \return device   a valid PCI device
 * \return NULL     if no device found
 */
struct ddekit_pci_dev const *
ddekit_pci_find_device(unsigned int *bus, unsigned int *slot, unsigned int *func,
                       struct ddekit_pci_dev const *start);

/** Enable PCI device
 * \ingroup DDEKit_pci
 */
int ddekit_pci_enable_device(struct ddekit_pci_dev *dev);

/** Disable PCI device
 * \ingroup DDEKit_pci
 */
int ddekit_pci_disable_device(struct ddekit_pci_dev *dev);

/** Enable bus-mastering for device.
 * \ingroup DDEKit_pci
 */
void ddekit_pci_set_master(struct ddekit_pci_dev *dev);

/** Get device vendor ID.
 * \ingroup DDEKit_pci
 */
unsigned short ddekit_pci_get_vendor(struct ddekit_pci_dev *dev);

/** Get device ID.
 * \ingroup DDEKit_pci
 */
unsigned short ddekit_pci_get_device_id(struct ddekit_pci_dev *dev);

/** Get device subvendor ID.
 * \ingroup DDEKit_pci
 */
unsigned short ddekit_pci_get_sub_vendor(struct ddekit_pci_dev *dev);

/** Get subdevice ID.
 * \ingroup DDEKit_pci
 */
unsigned short ddekit_pci_get_sub_device(struct ddekit_pci_dev *dev);

/** Get device class ID.
 * \ingroup DDEKit_pci
 */
unsigned       ddekit_pci_get_dev_class(struct ddekit_pci_dev *dev);

/** Get device's IRQ number.
 * \ingroup DDEKit_pci
 */
unsigned long  ddekit_pci_get_irq(struct ddekit_pci_dev *dev);

/** Get device name.
 * \ingroup DDEKit_pci
 */
char const *ddekit_pci_get_name(struct ddekit_pci_dev *dev);

/** Get device's slot name.
 * \ingroup DDEKit_pci
 */
char const *ddekit_pci_get_slot_name(struct ddekit_pci_dev *dev);

/** Get one of the device's resources.
 * \ingroup DDEKit_pci
 */
ddekit_pci_res_t *ddekit_pci_get_resource(struct ddekit_pci_dev *dev, unsigned int idx);

EXTERN_C_END
