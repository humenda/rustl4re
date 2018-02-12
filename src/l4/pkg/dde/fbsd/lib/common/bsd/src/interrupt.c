/**
 * Interrupt subsystem using Omega0. Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <machine/intr_machdep.h>

#include <dde_fbsd/thread.h>
#include <dde_fbsd/interrupt.h>
#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/interrupt.h>

#define dbg_this 0

struct interrupt {
	LIST_HEAD(handlers_head, int_handler) handlers;
	int flags;
	int vector;
	ddekit_thread_t *thread;
};

struct int_handler {
	LIST_ENTRY(int_handler) next;
	const char *name;
	driver_intr_t *handler;
	void *arg;
	struct interrupt *assigned_to;
};

static struct interrupt *interrupts[NUM_IO_INTS];

static ddekit_lock_t intr_lock;

static MALLOC_DEFINE(M_INTR, "int handlers", "interrupt handlers");

static void intr_init(void *arg) {
	bsd_thread_setup_myself();
}

static void intr_handler(void *arg) {
	struct interrupt *intr = arg;
	struct int_handler *ih;
	int vector = intr->vector;

	if (dbg_this) printf("recieved irq %d\n", vector);

	LIST_FOREACH(ih, &intr->handlers, next) {
		if (dbg_this) printf("calling handler\n");
		ih->handler(ih->arg);
	}
}

int intr_prepare(int vector) {
	struct interrupt *intr;
	int err;

	ddekit_lock_lock(&intr_lock);

	// look in interrupt table if already prepared
	intr = interrupts[vector];
	if (intr) {
		printf("irq %d already prepared\n", vector);
		ddekit_lock_unlock(&intr_lock);
		return 0;
	}

	// create new table entry
	intr = interrupts[vector] = (struct interrupt *) malloc(sizeof(struct interrupt), M_INTR, M_WAITOK);
	LIST_INIT(&intr->handlers);
	intr->flags  = 0;
	intr->vector = vector;
	intr->thread = ddekit_interrupt_attach(vector, 0, intr_init, intr_handler, intr);

	err = intr->thread?0:1;

	if (err) {
		printf("ERROR creating interrupt thread\n");
		free(intr, M_INTR);
		interrupts[vector] = NULL;
	}

	ddekit_lock_unlock(&intr_lock);

	return err;
}

int intr_add_handler(const char *name, int vector, driver_intr_t handler,
	void *arg, enum intr_type flags, void **cookiep)
{
	struct int_handler *ih;
	struct interrupt *intr;

	ddekit_lock_lock(&intr_lock);

	KASSERT(vector < NUM_IO_INTS, ("int vector (%d) too high (>=%d)", vector, NUM_IO_INTS));

	intr = interrupts[vector];
	KASSERT(intr, ("interrupt vector %d is not prepared", vector));

	if ((flags & INTR_EXCL) || (intr->flags & INTR_EXCL)) {
		KASSERT(LIST_EMPTY(&intr->handlers), ("INTR_EXCL but already a handler set"));
	}

	intr->flags  |= (flags&INTR_EXCL) ? INTR_EXCL : 0;

	ih = (struct int_handler *) malloc(sizeof(struct int_handler), M_INTR, M_WAITOK);
	ih->name    = name;
	ih->handler = handler;
	ih->arg     = arg;
	ih->assigned_to = intr;
	LIST_INSERT_HEAD(&intr->handlers, ih, next);

	if (cookiep)
		*cookiep = ih;

	ddekit_lock_unlock(&intr_lock);

	return 0;
}

int intr_config_intr(int vector, enum intr_trigger trig, enum intr_polarity pol) {
	printf("intr_config_intr(vector=%d trig=%d pol=%d)\n", vector, trig, pol);
	return 0;
}

int intr_remove_handler(void *cookie) {
	struct int_handler *ih = cookie;

	ddekit_lock_lock(&intr_lock);

	if (dbg_this) printf("intr_remove_handler(name=%s)\n", ih->name);

	LIST_REMOVE(ih, next);
	if (LIST_EMPTY(&ih->assigned_to->handlers)) {
		ih->assigned_to->flags = 0;
	}

	free(ih, M_INTR);

	ddekit_lock_unlock(&intr_lock);

	return 0;
}

static void intr_init_subsys(void *unused) {
	ddekit_lock_init(&intr_lock);
}
SYSINIT(intr, SI_DDE_INTR, SI_ORDER_FIRST, intr_init_subsys, NULL);
