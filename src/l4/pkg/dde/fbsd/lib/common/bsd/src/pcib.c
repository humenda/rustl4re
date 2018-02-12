/**
 * A pci bus driver using dde. Written from scratch, inspired by the various
 * FreeBSD pci drivers.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <pcib_if.h>

#include <l4/dde/ddekit/pci.h>

#define PCIB_DEBUG 0

/**
 * The highest slot number found.
 */
static int32_t maxslotnum;

// from dev/pci/pci_pci.c:091
devclass_t pcib_devclass;

static MALLOC_DEFINE(M_PCIBDEV, "pcibdev", "PCI bus device");
struct pcib_device {
	uint32_t bus;
};

#define GETPCIBDEV(dev) ((struct pcib_device *)device_get_ivars(dev))

static void      pcib_identify(driver_t *driver, device_t parent);
static int       pcib_probe(device_t dev);
static int       pcib_attach(device_t dev);
static int       pcib_detach(device_t dev);
static int       pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result);
static int       pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value);
static int       pcib_maxslots(device_t dev);
static u_int32_t pcib_read_config(device_t dev, int bus, int slot, int func, int reg, int bytes);
static void      pcib_write_config(device_t dev, int bus, int slot, int func, int reg, u_int32_t data, int bytes);
static int       pcib_route_interrupt(device_t pcib, device_t dev, int pin);

static device_method_t dde_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,         pcib_identify),
	DEVMETHOD(device_probe,            pcib_probe),
	DEVMETHOD(device_attach,           pcib_attach),
	DEVMETHOD(device_detach,           pcib_detach),
	DEVMETHOD(device_shutdown,         bus_generic_shutdown),
	DEVMETHOD(device_suspend,          bus_generic_suspend),
	DEVMETHOD(device_resume,           bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,         bus_generic_print_child),
 	DEVMETHOD(bus_read_ivar,           pcib_read_ivar),
 	DEVMETHOD(bus_write_ivar,          pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,      bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,    bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,   bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,          bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,       bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,           pcib_maxslots),
	DEVMETHOD(pcib_read_config,        pcib_read_config),
	DEVMETHOD(pcib_write_config,       pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,    pcib_route_interrupt),

	{ 0, 0 }
};

static driver_t dde_pcib_driver = {
        "pcib",
        dde_pcib_methods,
        1,
};

DRIVER_MODULE(pcib, nexus, dde_pcib_driver, pcib_devclass, 0, 0);

static void pcib_identify(driver_t *driver, device_t parent) {
	int bus, maxbusnum;
	int index;

	if (PCIB_DEBUG) printf("pcib_identify(parent=%s)\n", device_get_nameunit(parent));

	ddekit_pci_init();

	// iterate through pci devices and initialize maximum bus number and
	// maximum slot number of all busses
	maxbusnum = -1;
	maxslotnum = -1;

	// calc maxbusnum and maxslotnum
	for (index=0; ; index++) {
		int bus, slot, func;
		int err;

		err = ddekit_pci_get_device(index, &bus, &slot, &func);
		if (err) break;

		if (PCIB_DEBUG)
			printf("pcib_identify: found device: bus=%d, slot=%d fun=%d\n", bus, slot, func);

		// update statistics
		if (bus > maxbusnum)
			maxbusnum = bus;
		if (slot > maxslotnum)
			maxslotnum = slot;
	}

	if (maxbusnum<0)
		if (PCIB_DEBUG) printf("pcib_identify: no pci devices found\n");

	// add one child per bus
	for (bus=0; bus<=maxbusnum; bus++)
		BUS_ADD_CHILD(parent, 100, "pcib", /* unit number */ bus);
}

static int pcib_probe(device_t dev) {
	if (PCIB_DEBUG) printf("pcib_probe(parent=%s dev=%s)\n", device_get_nameunit(device_get_parent(dev)), device_get_nameunit(dev));
	return 0;
}

static int pcib_attach(device_t dev) {
	struct pcib_device *pcib;

	if (PCIB_DEBUG) printf("pcib_attach(parent=%s dev=%s)\n", device_get_nameunit(device_get_parent(dev)), device_get_nameunit(dev));

	// initialize ivars
	pcib = (struct pcib_device *) malloc(sizeof(struct pcib_device), M_PCIBDEV, M_ZERO|M_WAITOK);
	pcib->bus = device_get_unit(dev);
	device_set_ivars(dev, pcib);

	// add pci layer as child
	device_add_child(dev, "pci", pcib->bus);
	return bus_generic_attach(dev);
}

static int pcib_detach(device_t dev) {
	free(device_get_ivars(dev), M_PCIBDEV);
	return 0;
}

static int pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result) {
	if (PCIB_DEBUG) printf("pcib_read_ivar(dev=%s child=%s which=%d)\n", device_get_nameunit(dev), device_get_nameunit(child), which);
	switch (which) {
		case PCIB_IVAR_BUS:
			// in fbsd we would ask legacy, which would answer -1
			*result = GETPCIBDEV(dev)->bus;
			return 0;
	}
	return ENOENT;
}

static int pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value) {
	if (PCIB_DEBUG) printf("pcib_write_ivar(dev=%s child=%s which=%d value=%d)\n", device_get_nameunit(dev), device_get_nameunit(child), which, value);
	switch (which) {
		case PCIB_IVAR_BUS:
			GETPCIBDEV(dev)->bus = value;
			return 0;
	}
	return ENOENT;
}

static int pcib_maxslots(device_t dev) {
	if (PCIB_DEBUG) printf("pcib_maxslots(dev=%s)\n", device_get_nameunit(dev));
	return maxslotnum;
}

static u_int32_t pcib_read_config(device_t dev, int bus, int slot, int func, int reg, int bytes) {
	u_int32_t data;
	u_int16_t datw;
	u_int8_t  datb;
	int err;

	data=-1;
	switch (bytes) {
		case 1:
			err = ddekit_pci_readb(bus, slot, func, reg, &datb);
			if (err) return -1;
			data = datb;
			break;
		case 2:
			err = ddekit_pci_readw(bus, slot, func, reg, &datw);
			if (err) return -1;
			data = datw;
			break;
		case 4:
			err = ddekit_pci_readl(bus, slot, func, reg, &data);
			if (err) return -1;
			break;
	}
	return data;
}

static void pcib_write_config(device_t dev, int bus, int slot, int func, int reg, u_int32_t data, int bytes) {
	switch (bytes) {
		case 1:
			ddekit_pci_writeb(bus, slot, func, reg, data);
			break;
		case 2:
			ddekit_pci_writew(bus, slot, func, reg, data);
			break;
		case 4:
			ddekit_pci_writel(bus, slot, func, reg, data);
			break;
	}
}

static int pcib_route_interrupt(device_t pcib, device_t dev, int pin) {
	printf("pcib_route_interrupt(pcib=%s dev=(%d:%02d:%d %s) pin=%d)\n",
		device_get_nameunit(pcib),
		pci_get_bus(dev), pci_get_slot(dev), pci_get_function(dev), device_get_nameunit(dev),
		pin
	);

	return PCI_INVALID_IRQ;
}
