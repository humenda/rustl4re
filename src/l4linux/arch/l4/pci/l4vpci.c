#include <linux/pci.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <asm/pci.h>
#ifdef CONFIG_X86
#include <asm/pci_x86.h>
#endif

#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_pci.h>

#include <asm/l4lxapi/irq.h>
#include <asm/generic/l4lib.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/vcpu.h>

#include <l4/re/env.h>

L4_EXTERNAL_FUNC(l4vbus_pci_irq_enable);
L4_EXTERNAL_FUNC(l4vbus_pci_cfg_read);
L4_EXTERNAL_FUNC(l4vbus_pci_cfg_write);
L4_EXTERNAL_FUNC(l4vbus_get_device_by_hid);

static l4_cap_idx_t vbus;
static l4vbus_device_handle_t root_bridge;

#ifdef CONFIG_X86
unsigned int pci_probe;
unsigned int pci_early_dump_regs;
int noioapicquirk;
int noioapicreroute = 0;
int pci_routeirq;
int pcibios_last_bus = -1;
#endif

/*
 * l4vpci_pci_irq_enable
 * success: return 0
 * failure: return < 0
 */

static int l4vpci_irq_enable(struct pci_dev *dev)
{
	unsigned char trigger, polarity;
	int irq;
	u8 pin = 0;
	unsigned flags;
	l4_uint32_t devfn;

	if (!dev)
		return -EINVAL;

	if (dev->msi_enabled || dev->msix_enabled)
		return 0;

	pin = dev->pin;
	if (!pin) {
		dev_warn(&dev->dev,
		         "No interrupt pin configured for device %s\n",
		         pci_name(dev));
		return 0;
	}
	pin--;


	if (!dev->bus) {
		dev_err(&dev->dev, "invalid (NULL) 'bus' field\n");
		return -ENODEV;
	}

	devfn = (PCI_SLOT(dev->devfn) << 16) | PCI_FUNC(dev->devfn);
	irq = L4XV_FN_i(l4vbus_pci_irq_enable(vbus, root_bridge, dev->bus->number,
	                                      devfn, pin, &trigger, &polarity));
	if (irq < 0) {
		dev_warn(&dev->dev, "PCI INT %c: no GSI", 'A' + pin);
		/* Interrupt Line values above 0xF are forbidden */
		return 0;
	}

	switch ((!!trigger) | ((!!polarity) << 1)) {
		case 0: flags = IRQF_TRIGGER_HIGH; break;
		case 1: flags = IRQF_TRIGGER_RISING; break;
		case 2: flags = IRQF_TRIGGER_LOW; break;
		case 3: flags = IRQF_TRIGGER_FALLING; break;
		default: flags = 0; break;
	}

	dev->irq = irq;
	l4lx_irq_set_type(irq_get_irq_data(irq), flags);

	dev_info(&dev->dev, "PCI INT %c -> GSI %u (%s, %s) -> IRQ %d\n",
	         'A' + pin, irq,
	         !trigger ? "level" : "edge",
	         polarity ? "low" : "high", dev->irq);

	return 0;
}

void l4vpci_irq_disable(struct pci_dev *dev)
{
	dev_info(&dev->dev, "%s: implement me\n", __func__);
}

/*
 * Functions for accessing PCI base (first 256 bytes) and extended
 * (4096 bytes per PCI function) configuration space with type 1
 * accesses.
 */

int raw_pci_read(unsigned int domain, unsigned int bus,
                 unsigned int devfn, int reg, int len, u32 *value)
{
	l4_uint32_t df = (PCI_SLOT(devfn) << 16) | PCI_FUNC(devfn);
	return L4XV_FN_i(l4vbus_pci_cfg_read(vbus, root_bridge,
	                                     bus, df, reg, value, len * 8));
}

int raw_pci_write(unsigned int domain, unsigned int bus,
                  unsigned int devfn, int reg, int len, u32 value)
{
	l4_uint32_t df = (PCI_SLOT(devfn) << 16) | PCI_FUNC(devfn);
	return L4XV_FN_i(l4vbus_pci_cfg_write(vbus, root_bridge,
	                                      bus, df, reg, value, len * 8));
}

static int pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
	return raw_pci_read(pci_domain_nr(bus), bus->number,
	                    devfn, where, size, value);
}

static int pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
	return raw_pci_write(pci_domain_nr(bus), bus->number,
	                     devfn, where, size, value);
}

static struct pci_ops l4vpci_ops = {
	.read = pci_read,
	.write = pci_write,
};


#ifdef CONFIG_ARM
static int __init l4vpci_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	(void)slot;
	(void)pin;
	printk("l4vpci_map_irq: %d/%d\n", slot, pin);
        //l4vpci_irq_enable(dev);
	return dev->irq;
}

int __init l4vpci_setup(int nr, struct pci_sys_data *sys)
{
	(void)sys;
	pcibios_setup("firmware");
	return nr == 0 ? 1 : 0;
}

struct pci_bus * __init l4vpci_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct pci_bus *b;
	struct pci_dev *dev = NULL;
	b = pci_scan_root_bus(NULL, sys->busnr, &l4vpci_ops, sys,
	                      &sys->resources);
	for_each_pci_dev(dev)
		l4vpci_irq_enable(dev);

	pci_bus_add_devices(b);

	return b;
}

static struct hw_pci l4vpci_pci __initdata = {
	.map_irq                = l4vpci_map_irq,
	.nr_controllers         = 1,
	.setup                  = l4vpci_setup,
	.scan                   = l4vpci_scan_bus,
};
#endif

#ifdef CONFIG_X86
static int __init l4vpci_x86_init(void)
{
	struct pci_dev *dev = NULL;
	struct pci_bus *bus;
	struct pci_sysdata *sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	bus = pci_scan_bus(0, &l4vpci_ops, sd);
	if (!bus) {
		pr_err("Failed to scan PCI bus\n");
		return -ENODEV;
	}

	pci_bus_add_devices(bus);

	pr_info("l4vPCI: Using L4-IO for IRQ routing\n");

	for_each_pci_dev(dev)
		l4vpci_irq_enable(dev);

#ifdef CONFIG_X86
	pcibios_resource_survey();
#endif

	return 0;
}

unsigned int pcibios_assign_all_busses(void)
{
	return 1;
}
#endif

int pcibios_enable_device(struct pci_dev *dev, int bars)
{
	int err;

	if ((err = pci_enable_resources(dev, bars)) < 0)
		return err;

	return l4vpci_irq_enable(dev);
}

int early_pci_allowed(void)
{
	return 0;
}

void early_dump_pci_devices(void)
{
	printk("%s: unimplemented\n", __func__);
}

u32 read_pci_config(u8 bus, u8 slot, u8 func, u8 offset)
{
	u32 val;
	if (L4XV_FN_i(l4vbus_pci_cfg_read(vbus, root_bridge,
	                                  bus, (slot << 16) | func,
	                                  offset, &val, 32)))
		return 0;
	return val;
}

u8 read_pci_config_byte(u8 bus, u8 slot, u8 func, u8 offset)
{
	u32 val;
	if (L4XV_FN_i(l4vbus_pci_cfg_read(vbus, root_bridge,
	                                  bus, (slot << 16) | func,
	                                  offset, &val, 8)))
		return 0;
	return val;
}

u16 read_pci_config_16(u8 bus, u8 slot, u8 func, u8 offset)
{
	u32 val;
	if (L4XV_FN_i(l4vbus_pci_cfg_read(vbus, root_bridge,
	                                  bus, (slot << 16) | func,
	                                  offset, &val, 16)))
		return 0;
	return val;
}

void write_pci_config(u8 bus, u8 slot, u8 func, u8 offset, u32 val)
{
	L4XV_FN_v(l4vbus_pci_cfg_write(vbus, root_bridge,
	                               bus, (slot << 16) | func,
	                               offset, val, 32));
}

void write_pci_config_byte(u8 bus, u8 slot, u8 func, u8 offset, u8 val)
{
	L4XV_FN_v(l4vbus_pci_cfg_write(vbus, root_bridge,
	                               bus, (slot << 16) | func,
	                               offset, val, 8));
}

void write_pci_config_16(u8 bus, u8 slot, u8 func, u8 offset, u16 val)
{
	L4XV_FN_v(l4vbus_pci_cfg_write(vbus, root_bridge,
	                               bus, (slot << 16) | func,
	                               offset, val, 16));
}

#ifdef CONFIG_X86
char * pcibios_setup(char *str)
{
	return str;
}
#endif

void pcibios_disable_device(struct pci_dev *dev)
{
	l4vpci_irq_disable(dev);
}

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */
void pcibios_fixup_bus(struct pci_bus *b)
{
	pci_read_bridge_bases(b);
}

void pcibios_scan_root(int busnum)
{
}

int __init pci_legacy_init(void)
{
	return 0;
}

void __init pcibios_irq_init(void)
{
}

void __init pcibios_fixup_irqs(void)
{
}

static int __init l4vpci_init(void)
{
	int err;

	vbus = l4re_env_get_cap("vbus");
	if (l4_is_invalid_cap(vbus))
		return -ENOENT;

	err = L4XV_FN_i(l4vbus_get_device_by_hid(vbus, 0, &root_bridge,
	                                         "PNP0A03", 0, 0));
	if (err < 0) {
		printk(KERN_INFO "l4vPCI: no root bridge found, no PCI\n");
		return err;
	}

	printk(KERN_INFO "l4vPCI: L4 root bridge is device %lx\n", root_bridge);

#ifdef CONFIG_X86
	return l4vpci_x86_init();
#endif

#ifdef CONFIG_ARM
	pci_common_init(&l4vpci_pci);
	return 0;
#endif
}

subsys_initcall(l4vpci_init);
