/**
 * Derived from i386/i386/nexus.c .
 * \author Thomas Friebel <tf13@os.int.fu-dresden.de>
 */
/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/i386/nexus.c,v 1.58.2.2 2004/11/07 22:35:36 njl Exp $");

/*
 * This code implements a `root nexus' for Intel Architecture
 * machines.  The function of the root nexus is to serve as an
 * attachment point for both processors and buses, and to manage
 * resources which are common to all of them.  In particular,
 * this code implements the core resource managers for interrupt
 * requests, DMA requests (which rightfully should be a part of the
 * ISA code but it's easier to do it here for now), I/O port addresses,
 * and I/O memory address space.
 */

#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <machine/resource.h>

#ifdef DEV_ISA
#include <isa/isavar.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#endif
#include <sys/rtprio.h>

#include <dde_fbsd/interrupt.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/resources.h>

#define dbg_this 0

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");
struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman irq_rman, drq_rman, port_rman, mem_rman;

static	int nexus_probe(device_t);
static	int nexus_attach(device_t);
static	int nexus_print_all_resources(device_t dev);
static	int nexus_print_child(device_t, device_t);
static device_t nexus_add_child(device_t bus, int order, const char *name,
				int unit);
static	struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
					      u_long, u_long, u_long, u_int);
static	int nexus_config_intr(device_t, int, enum intr_trigger,
			      enum intr_polarity);
static	int nexus_activate_resource(device_t, device_t, int, int,
				    struct resource *);
static	int nexus_deactivate_resource(device_t, device_t, int, int,
				      struct resource *);
static	int nexus_release_resource(device_t, device_t, int, int,
				   struct resource *);
static	int nexus_setup_intr(device_t, device_t, struct resource *, int flags,
			     void (*)(void *), void *, void **);
static	int nexus_teardown_intr(device_t, device_t, struct resource *,
				void *);
static struct resource_list *nexus_get_reslist(device_t dev, device_t child);
static	int nexus_set_resource(device_t, device_t, int, int, u_long, u_long);
static	int nexus_get_resource(device_t, device_t, int, int, u_long *, u_long *);
static void nexus_delete_resource(device_t, device_t, int, int);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_get_resource_list, nexus_get_reslist),
	DEVMETHOD(bus_set_resource,	nexus_set_resource),
	DEVMETHOD(bus_get_resource,	nexus_get_resource),
	DEVMETHOD(bus_delete_resource,	nexus_delete_resource),

	{ 0, 0 }
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1,			/* no softc */
};
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static int
nexus_probe(device_t dev)
{
	device_quiet(dev);	/* suppress attach message for neatness */

	// irq's
	irq_rman.rm_start = 0;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupt request lines";
	irq_rman.rm_end = NUM_IO_INTS - 1;
	if (rman_init(&irq_rman))
		panic("nexus_probe irq_rman");
	if (rman_manage_region(&irq_rman, 0, NUM_IO_INTS - 1) != 0)
		panic("nexus_probe irq_rman add");

	// no drqs
	drq_rman.rm_start = 0;
	drq_rman.rm_type = RMAN_ARRAY;
	drq_rman.rm_descr = "DMA request lines";
	irq_rman.rm_end = 3;
	if (rman_init(&drq_rman))
		panic("nexus_probe drq_rman");
	if (rman_manage_region(&drq_rman, drq_rman.rm_start, drq_rman.rm_end))
		panic("nexus_probe drq_rman");

	port_rman.rm_start = 0;
	port_rman.rm_end = 0xffff;
	port_rman.rm_type = RMAN_ARRAY;
	port_rman.rm_descr = "I/O ports";
	if (rman_init(&port_rman)
	    || rman_manage_region(&port_rman, 0, 0xffff))
		panic("nexus_probe port_rman");

	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0u;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman)
	    || rman_manage_region(&mem_rman, 0, ~0))
		panic("nexus_probe mem_rman");

	return 0;
}

static int
nexus_attach(device_t dev)
{

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return 0;
}

static int
nexus_print_all_resources(device_t dev)
{
	struct	nexus_device *ndev = DEVTONX(dev);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	if (SLIST_FIRST(rl))
		retval += printf(" at");
	
	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return retval;
}

static int
nexus_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += nexus_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on motherboard\n");	/* XXX "motherboard", ick */

	return (retval);
}

static device_t
nexus_add_child(device_t bus, int order, const char *name, int unit)
{
	device_t		child;
	struct nexus_device	*ndev;

	ndev = malloc(sizeof(struct nexus_device), M_NEXUSDEV, M_NOWAIT|M_ZERO);
	if (!ndev)
		return(0);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(bus, order, name, unit); 

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return(child);
}

static char *get_restype_name(int type) {
	char *stype="unknown";
	switch (type) {
		case SYS_RES_IRQ:
			stype="IRQ";
			break;
		case SYS_RES_DRQ:
			stype="DRQ";
			break;
		case SYS_RES_MEMORY:
			stype="MEM";
			break;
		case SYS_RES_IOPORT:
			stype="I/O";
			break;
	}
	return stype;
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 * (Exceptions include npx.)
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct	resource *rv;
	struct resource_list_entry *rle;
	struct	rman *rm;
	int needactivate = flags & RF_ACTIVE;

	if (dbg_this)
		printf("nexus_alloc_resource(%s 0x%08lx-0x%08lx flags=0x%04x)\n", get_restype_name(type), start, end, flags);

	/*
	 * If this is an allocation of the "default" range for a given RID, and
	 * we know what the resources for this device are (ie. they aren't maintained
	 * by a child bus), then work out the start/end values.
	 */
	if ((start == 0UL) && (end == ~0UL) && (count == 1)) {
		if (ndev == NULL)
			return(NULL);
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return(NULL);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	flags &= ~RF_ACTIVE;

	// use the responsible resource manager
	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;
	case SYS_RES_DRQ:
		rm = &drq_rman;
		break;
	case SYS_RES_IOPORT:
		rm = &port_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;
	default:
		return 0;
	}
	// reserve the resource local
	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0)
		return 0;

	KASSERT(count==end-start+1, ("count (%d) != start (%d) - end (%d) + 1", count, start, end));

	// ??? what is this for?
	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, I386_BUS_SPACE_MEM);
	} else if (type == SYS_RES_IOPORT) {
		rman_set_bustag(rv, I386_BUS_SPACE_IO);
		rman_set_bushandle(rv, rman_get_start(rv));
	}

	// activate if requested
	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return 0;
		}
	}
	
	return rv;
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	u_long start = rman_get_start(r);
	u_long end   = rman_get_end(r);
	u_long count = rman_get_size(r);

	if (dbg_this) printf("nexus_activate_resource(child=%s type=%s range=0x%08lux-0x%08lux)\n",
			device_get_nameunit(child), get_restype_name(type),
			rman_get_start(r), rman_get_end(r));
	// do the reservation at the i/o server
	if (type==SYS_RES_IRQ) {
		int err;

		KASSERT(count==1, ("trying to allocate more than one dma channel at once"));

		err = intr_prepare(start);
		if (err) {
			printf("nexus: error allocating irq\n");
			return EBUSY;
		}
	} else if (type==SYS_RES_DRQ) {
		int err;

		KASSERT(count==1, ("trying to allocate more than one dma channel at once"));

		err = ddekit_request_dma(start);
		if (err) {
			printf("nexus: error allocating isa dma channels\n");
			return EBUSY;
		}
	} else if (type==SYS_RES_IOPORT) {
		int err;

		err = ddekit_request_io(start, count);
		if (err) {
			printf("nexus: error allocating io ports\n");
			return EBUSY;
		}
	} else if (type==SYS_RES_MEMORY) {
		int err;
		ddekit_addr_t virtual;

		if (end < 1024 * 1024) {
//			/*
//			 * The first 1Mb is mapped at KERNBASE.
//			 */
//			vaddr = (caddr_t)(uintptr_t)(KERNBASE + rman_get_start(r));
			ddekit_debug("special case? review (+implement)!");
		}

		err = ddekit_request_mem(start, count, &virtual);
		if (err) {
			printf("nexus: error allocating device memory\n");
			return EBUSY;
		}
		if (dbg_this) printf("nexus: mapped at virt=0x%08x\n", virtual);

		//XXXY: setup virtual->bus page mapping? (ddekit_set_pte)

		rman_set_virtual(r, (void*) virtual);
		rman_set_bushandle(r, (bus_space_handle_t) virtual);
	}

	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	if (dbg_this) printf("nexus_deactivate_resource(child=%s type=%s range=0x%08lux-0x%08lux)\n",
			device_get_nameunit(child), get_restype_name(type),
			rman_get_start(r), rman_get_end(r));

	// undo the reservation at the i/o server
	if (type==SYS_RES_IRQ) {
		ddekit_debug("irq release not yet implemented");
	} else if (type==SYS_RES_DRQ) {
		int err;

		err = ddekit_release_dma(rman_get_start(r));
		KASSERT(!err, ("error releasing dma channel"));
	} else if (type==SYS_RES_IOPORT) {
		int err;

		err = ddekit_release_io(rman_get_start(r), rman_get_size(r));
		KASSERT(!err, ("error releasing io ports"));
	} else if (type==SYS_RES_MEMORY) {
		int err;

		err = ddekit_release_mem(rman_get_start(r), rman_get_size(r));
		KASSERT(!err, ("error releasing device memory"));
	}

	return (rman_deactivate_resource(r));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if (dbg_this) printf("nexus_release_resource(child=%s type=%s range=0x%08lux-0x%08lux)\n",
			device_get_nameunit(child), get_restype_name(type),
			rman_get_start(r), rman_get_end(r));

	// deactivate first if active
	if (rman_get_flags(r) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}

	return (rman_release_resource(r));
}

/*
 * Currently this uses the really grody interface from kern/kern_intr.c
 * (which really doesn't belong in kern/anything.c).  Eventually, all of
 * the code in kern_intr.c and machdep_intr.c should get moved here, since
 * this is going to be the official interface.
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
		 int flags, void (*ihand)(void *), void *arg, void **cookiep)
{
	int		error;

	/* somebody tried to setup an irq that failed to allocate! */
	if (irq == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if ((rman_get_flags(irq) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	rman_activate_resource(irq);

	error = intr_add_handler(device_get_nameunit(child),
	    rman_get_start(irq), ihand, arg, flags, cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t bus, device_t child, struct resource *r, void *ih)
{
	return (intr_remove_handler(ih));;
}

static int
nexus_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	return (intr_config_intr(irq, trig, pol));
}

static struct resource_list *
nexus_get_reslist(device_t dev, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);

	return (&ndev->nx_resources);
}

static int
nexus_set_resource(device_t dev, device_t child, int type, int rid, u_long start, u_long count)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	/* XXX this should return a success/failure indicator */
	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return(0);
}

static int
nexus_get_resource(device_t dev, device_t child, int type, int rid, u_long *startp, u_long *countp)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return(ENOENT);
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return(0);
}

static void
nexus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct nexus_device	*ndev = DEVTONX(child);
	struct resource_list	*rl = &ndev->nx_resources;

	resource_list_delete(rl, type, rid);
}

