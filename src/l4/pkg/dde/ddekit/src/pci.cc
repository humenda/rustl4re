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

#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/pci.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/interrupt.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/sys/kdebug.h>

#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_pci.h>

#include <cstdlib>
#include <list>

#include "config.h"
#include "internals.hh"

#if 0
const unsigned ddekit_pci_dev::RES_MAX = 12;
const unsigned DDEKit::Pci_bus::MAX_DEVS = 12;
#endif

typedef struct ddekit_pci_dev ddekit_pci_dev_t;

DDEKit::Pci_bus *ddekit_pci_bus;

static l4_cap_idx_t _vbus;
static l4vbus_device_handle_t _root_bridge;

DDEKit::Pci_bus::Pci_bus()
{
	ddekit_printf("pci bus constructor\n");

	int err;

	_vbus = l4re_env_get_cap("vbus");
	if (l4_is_invalid_cap(_vbus)) {
		ddekit_printf("PCI: failed to query vbus\n");
		return;
	}

	err = l4vbus_get_device_by_hid(_vbus, 0, &_root_bridge, "PNP0A03", 0, 0);

	if (err < 0) {
		ddekit_printf("PCI: no root bridge found, no PCI\n");
		return;
	}

	ddekit_printf("PCI: L4 root bridge is device %x\n", _root_bridge);

	return;
}


DDEKit::Pci_bus::~Pci_bus()
{
}


#if 0
EXTERN_C int ddekit_pci_get_device(int nr, int *bus, int *slot, int *func)
{
	ddekit_printf("i don't know what this does...\n");
	return -1;
}
#endif

#if 0
ddekit_pci_dev::ddekit_pci_dev(l4io_pci_dev_t *dev)
	: _dev(*dev), /*_irq(0),*/ _cap(L4_INVALID_CAP), _slot_name(0)
{
	_resources = static_cast<ddekit_pci_res**>(ddekit_simple_malloc(sizeof(ddekit_pci_res) * RES_MAX));
	std::memset(_resources, 0, sizeof(_resources));
	for (unsigned i = 0; i < dev->num_resources; ++i)
	{
		_resources[i] = new ddekit_pci_res(&dev->resources[i]);
	}
}



ddekit_pci_dev::~ddekit_pci_dev()
{
	for (unsigned i = 0; i < _dev.num_resources; ++i)
		delete _resources[i];
	ddekit_printf("deleted %d resources.\n", _dev.num_resources);

	ddekit_simple_free(_resources);
}

ddekit_pci_res const * ddekit_pci_dev::resource(unsigned idx) const
{
	if (idx < _dev.num_resources)
		return _resources[idx];

	return 0;
}


EXTERN_C void ddekit_pci_dev::enable() const
{
	if (l4io_pci_enable(_dev.handle))
		ddekit_panic("%s failed\n", __FUNCTION__);
}

EXTERN_C int ddekit_pci_enable_device(struct ddekit_pci_dev *dev)
{
	dev->enable();
	return 0;
}

EXTERN_C void ddekit_pci_dev::disable()  const
{
	if (l4io_pci_disable(_dev.handle))
		ddekit_panic("%s failed\n", __FUNCTION__);
}

EXTERN_C int ddekit_pci_disable_device(struct ddekit_pci_dev *dev)
{
	dev->disable();
	return 0;
}

EXTERN_C void ddekit_pci_dev::set_master() const
{
	if (l4io_pci_set_master(_dev.handle))
		ddekit_panic("%s failed\n", __FUNCTION__);
}

EXTERN_C void ddekit_pci_set_master(struct ddekit_pci_dev *dev)
{
	dev->set_master();
}


/**
 * \brief Emulate PCI config space accesses.
 *
 * There are some ways, where we want to emulate reads in
 * cfg space, even if the device is not mapped to us yet. This allows
 * legacy code to use read_cfg() for getting device info while
 * we don't need to request a cap for this from L4Io yet.
 */

unsigned char ddekit_pci_dev::emulate_config_byte(unsigned pos) const
{
	enum values
	{
		CLASS_LOW   = 0x09,
		HEADER_TYPE = 0x0E,
		IRQ_LINE    = 0x3C,
		IRQ_PIN     = 0x3D,
	};

#if 0
	ddekit_printf("READ CFG BYTE EMULATION\n");
	ddekit_printf("\tPOS: %x, I am %p\n", pos, this);
#endif

	switch(pos)
	{
		case CLASS_LOW:
			return dev_class() & 0x08;
		case HEADER_TYPE:
			return this->header_type();
		case IRQ_LINE:
		case IRQ_PIN:
			// set to a sane value later...
			return 0;
		default:
			ddekit_printf("Requested emulation of unknown cfg byte position %x\n", pos);
			ddekit_panic("read_cfg_byte");
	}

	return 0;
}


unsigned short ddekit_pci_dev::emulate_config_word(unsigned pos) const
{
	enum values
	{
		STATUS = 0x06,
		SUB_VENDOR = 0x2C,
		SUB_SYS = 0x2E,
	};

#if 0
	ddekit_printf("READ CFG WORD EMULATION\n");
	ddekit_printf("\tPOS: %x, I am %p\n", pos, this);
#endif

	switch(pos)
	{
		case STATUS:
			/* Status cannot be emulated, because it may change over time.
			 * If a driver needs access to this state, it needs to get a 
			 * capability before. We just return 0 here.
			 *
			 * XXX: Linux init code uses some bits from the status register
			 *      to find out whether the device is PCI-X or plain PCI.
			 *      We don't support PCI-X so far and just assume PCI.
			 */
			return 0;
		case SUB_VENDOR:
			return sub_vendor();
		case SUB_SYS:
			return sub_device();
		default:
			ddekit_printf("Requested emulation of unknown cfg word position %x\n", pos);
			ddekit_panic("read_cfg_word");
	}

	return 0;
}


unsigned ddekit_pci_dev::emulate_config_dword(unsigned pos) const
{
	enum values
	{
		VENDOR_AND_DEVICE = 0x00,
		STATUS_AND_COMMAND = 0x04,
		CLASS_AND_REVISION = 0x08,
		BAR1 = 0x10,
		BAR2 = 0x14,
		BAR3 = 0x18,
		BAR4 = 0x1C,
		BAR5 = 0x20,
		BAR6 = 0x24,
		CARDBUS = 0x28,
		SUBSYS_VENDOR = 0x2C,
		XROM = 0x30,
	};

#if 0
	ddekit_printf("READ CFG DWORD EMULATION\n");
	ddekit_printf("\tPOS: %x, I am %p\n", pos, this);
#endif

	switch(pos)
	{
		case VENDOR_AND_DEVICE:
			//  0-15 - vendor ID
			// 16-31 - device ID
			return (vendor() | (device() << 16));
			break;
		case CLASS_AND_REVISION:
			// 0- 8 - revision, we leave it 0
			// 9-31 - class
			return dev_class() << 8;
		case BAR1:
		case BAR2:
		case BAR3:
		case BAR4:
		case BAR5:
		case BAR6:
#if 0
			ddekit_printf("BAR. returning 0xffffffff\n");
#endif
			return ~0;
		case XROM:
			return 0;
		default:
			ddekit_printf("Requested emulation of unknown cfg position %x\n", pos);
			ddekit_panic("read_cfg_dword");
	}

	return 0;
}
#endif

EXTERN_C int ddekit_pci_read(int bus, int slot, int func, int pos, int len, ddekit_uint32_t *val)
{
  l4_uint32_t devfn = (slot << 16) | func;
  return l4vbus_pci_cfg_read(_vbus, _root_bridge, bus, devfn, pos, val, len*8);
}

EXTERN_C int ddekit_pci_write(int bus, int slot, int func, int pos, int len, ddekit_uint32_t val)
{
  l4_uint32_t devfn = (slot << 16) | func;
  return l4vbus_pci_cfg_write(_vbus, _root_bridge, bus, devfn, pos, val, len*8);
}

#if 0
unsigned char ddekit_pci_dev::read_cfg_byte(unsigned pos) const
{
	if (l4_is_invalid_cap(_cap))
		return this->emulate_config_byte(pos);
	else
	{
		unsigned char ret = 0xff;
		int err = l4io_pci_readb_cfg(_dev.handle, pos, &ret);
		if (err)
			ddekit_printf("l4io_pci_readb_cfg failed (%d)", err);
		return ret;
	}
}
#endif

EXTERN_C int ddekit_pci_irq_enable(int bus, int slot, int func, int pin, int *irq)
{
  	unsigned char trigger;
	unsigned char polarity;
  	unsigned flags = 0;
	l4_uint32_t devfn = (slot << 16) | func;

	DEBUG_MSG("devfn %lx, pin %lx", devfn, pin);
	*irq = l4vbus_pci_irq_enable(_vbus, _root_bridge, bus, devfn, pin, &trigger, &polarity);
	DEBUG_MSG("l4vbus_pci_irq_enable() = %d", *irq);
	
	if (*irq < 0) {
		return -1;
	}

	switch ((!!trigger) | ((!!polarity) << 1)) {
		case 0: flags = IRQF_TRIGGER_HIGH; break;
		case 1: flags = IRQF_TRIGGER_RISING; break;
		case 2: flags = IRQF_TRIGGER_LOW; break;
		case 3: flags = IRQF_TRIGGER_FALLING; break;
		default: flags = 0; break;
	}

	// register the trigger type in the interrupt sysbsystem
	return ddekit_irq_set_type(*irq, flags);
}

#if 0
EXTERN_C unsigned short ddekit_pci_get_vendor(struct ddekit_pci_dev *dev)
{
	return dev->vendor();
}


EXTERN_C unsigned short ddekit_pci_get_device_id(struct ddekit_pci_dev *dev)
{
	return dev->device();
}


EXTERN_C unsigned short ddekit_pci_get_sub_vendor(struct ddekit_pci_dev *dev)
{
	return dev->sub_vendor();
}


EXTERN_C unsigned short ddekit_pci_get_sub_device(struct ddekit_pci_dev *dev)
{
	return dev->sub_device();
}


EXTERN_C unsigned       ddekit_pci_get_dev_class(struct ddekit_pci_dev *dev)
{
	return dev->dev_class();
}


EXTERN_C unsigned long  ddekit_pci_get_irq(struct ddekit_pci_dev *dev __attribute__((unused)))
{
	ddekit_debug("irq()");
	return 0;
//	return dev->irq();
}


EXTERN_C char const *ddekit_pci_get_name(struct ddekit_pci_dev *dev)
{
	return dev->name();
}


EXTERN_C char const *ddekit_pci_get_slot_name(struct ddekit_pci_dev *dev)
{
	return dev->slot_name();
}
#endif


EXTERN_C void ddekit_pci_init()
{
	ddekit_printf("%s\n", __func__);
	ddekit_pci_bus = new DDEKit::Pci_bus();
	if (l4_is_invalid_cap(_vbus)) {
		ddekit_printf("Failed to create pci bus\n");
		delete ddekit_pci_bus;
	}
}
