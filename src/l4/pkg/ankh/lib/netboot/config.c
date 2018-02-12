/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include	"grub.h"
#include	"nic.h"
#ifdef ANKH
#include <l4/sys/kdebug.h>
#include <stdio.h>
#include    "ankh_netboot.h"
#endif

static const char *driver_name[] = {
	"nic", 
	"disk", 
	"floppy",
};

#ifdef ANKH
extern int ankh_probe(struct dev* dev);
#endif

int probe(struct dev *dev)
{
	const char *type_name;

	EnterFunction("probe");

	type_name = "";
	if ((dev->type >= 0) && 
		(dev->type < sizeof(driver_name)/sizeof(driver_name[0]))) {
		type_name = driver_name[dev->type];
	}
	if (dev->how_probe == PROBE_FIRST) {
		dev->to_probe = PROBE_ANKH;
		memset(&dev->state, 0, sizeof(dev->state));
	}
    if (dev->to_probe == PROBE_ANKH) {
		printf("ankh_probe()\n");
		dev->how_probe = ankh_probe(dev);
		printf("return: %d\n", dev->how_probe);
		if (dev->how_probe == PROBE_FAILED)
			dev->to_probe = PROBE_NONE;
    }
	else {
		dev->how_probe = PROBE_FAILED;
	}

	LeaveFunction("probe");
	return dev->how_probe;
}

void disable(struct dev *dev)
{
	if (dev->disable) {
		dev->disable(dev);
		dev->disable = 0;
	}
}
