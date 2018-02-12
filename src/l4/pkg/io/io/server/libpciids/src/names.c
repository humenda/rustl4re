/*
 *	PCI Class and Device Name Tables
 *
 *	Copyright 1993--1999 Drew Eckhardt, Frederic Potter,
 *	David Mosberger-Tang, Martin Mares
 */

//#include <linux/types.h>
//#include <linux/kernel.h>
//#include <linux/pci.h>

#include <stdio.h>
#include <l4/io/pciids.h>

struct pci_device_info {
	unsigned short device;
	unsigned short seen;
	const char *name;
};

struct pci_vendor_info {
	unsigned short vendor;
	unsigned short nr;
	const char *name;
	struct pci_device_info *devices;
};

#define __devinitdata

/*
 * This is ridiculous, but we want the strings in
 * the .init section so that they don't take up
 * real memory.. Parse the same file multiple times
 * to get all the info.
 */
#define VENDOR( vendor, name )		static char __vendorstr_##vendor[] __devinitdata = name;
#define ENDVENDOR()
#define DEVICE( vendor, device, name ) 	static char __devicestr_##vendor##device[] __devinitdata = name;
#include "devlist.h"


#define VENDOR( vendor, name )		static struct pci_device_info __devices_##vendor[] __devinitdata = {
#define ENDVENDOR()			};
#define DEVICE( vendor, device, name )	{ 0x##device, 0, __devicestr_##vendor##device },
#include "devlist.h"

static struct pci_vendor_info __devinitdata pci_vendor_list[] = {
#define VENDOR( vendor, name )		{ 0x##vendor, sizeof(__devices_##vendor) / sizeof(struct pci_device_info), __vendorstr_##vendor, __devices_##vendor },
#define ENDVENDOR()
#define DEVICE( vendor, device, name )
#include "devlist.h"
};

#define VENDORS (sizeof(pci_vendor_list)/sizeof(struct pci_vendor_info))

//void libpci_name_device(char *name, struct pci_dev *dev)
void libpciids_name_device(char *name, int len,
                           unsigned vendor, unsigned device)
{
	const struct pci_vendor_info *vendor_p = pci_vendor_list;
	int i = VENDORS;

	do {
		if (vendor_p->vendor == vendor)
			goto match_vendor;
		vendor_p++;
	} while (--i);

	/* Couldn't find either the vendor nor the device */
	snprintf(name, len, "PCI device %04x:%04x", vendor, device);
        name[len - 1] = 0;
	return;

	match_vendor: {
		struct pci_device_info *device_p = vendor_p->devices;
		int i = vendor_p->nr;

		while (i > 0) {
			if (device_p->device == device)
				goto match_device;
			device_p++;
			i--;
		}

		/* Ok, found the vendor, but unknown device */
		snprintf(name, len, "PCI device %04x:%04x (%s)", vendor, device, vendor_p->name);
                name[len - 1] = 0;
		return;

		/* Full match */
		match_device: {
			int w = snprintf(name, len, "%s %s", vendor_p->name, device_p->name);
			int nr = device_p->seen + 1;
			device_p->seen = nr;
			if (nr > 1)
				snprintf(name + w, len - w, " (#%d)", nr);
                        name[len - 1] = 0;
		}
	}
}

/*
 *  Class names. Not in .init section as they are needed in runtime.
 */

#if 0
static u16 pci_class_numbers[] = {
#define CLASS(x,y) 0x##x,
#include "classlist.h"
};

static char *pci_class_names[] = {
#define CLASS(x,y) y,
#include "classlist.h"
};

char *
libpci_class_name(u32 class)
{
	int i;

	for(i=0; i<sizeof(pci_class_numbers)/sizeof(pci_class_numbers[0]); i++)
		if (pci_class_numbers[i] == class)
			return pci_class_names[i];
	return NULL;
}
#endif
