/**
 * The device autoconfiguration sysinits inspired by
 * i386/i386/autoconf.c
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dde_fbsd/cold.h>

static void ac_start(void *dummy) {
	// nexus0 is the top of the i386 device tree
	device_add_child(root_bus, "nexus", 0);

	// initiate autoconf
	root_bus_configure();
}

static void ac_done(void *dummy) {
	// start scheduling
	bsd_unset_cold();
}

SYSINIT(ac_start, SI_SUB_CONFIGURE, SI_ORDER_THIRD, ac_start, NULL);
SYSINIT(ac_done,  SI_SUB_CONFIGURE, SI_ORDER_ANY,   ac_done,  NULL);

