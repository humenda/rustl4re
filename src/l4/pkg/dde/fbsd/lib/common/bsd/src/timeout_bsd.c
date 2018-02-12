/**
 * Functions needed for using BSD's callout implementation.
 * Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/kthread.h>

#include <l4/dde/ddekit/thread.h>

MALLOC_DECLARE(M_CALLWHEEL);
MALLOC_DEFINE(M_CALLWHEEL, "callwheel", "the callwheel");

int ncallout = 50; // assumed number of callouts at a single point in time

static void tick_loop(void *arg) {
	while (1) {
		softclock(NULL);
		// sleep about the time of a tick
		ddekit_thread_msleep(1000/hz);
	}
}

static void init_timeout_loop(void *foo __unused) {
	caddr_t size;

	size = kern_timeout_callwheel_alloc((caddr_t) 0);
	size = (caddr_t) malloc((int) size, M_CALLWHEEL, M_WAITOK);
	kern_timeout_callwheel_alloc(size);
	kern_timeout_callwheel_init();

	kthread_create(tick_loop, NULL, NULL, 0, 0, "ticker");
}
SYSINIT(timeout_loop, SI_DDE_TIMEOUT, SI_ORDER_FIRST, &init_timeout_loop, NULL);
