#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <dde_fbsd/cold.h>
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/lock.h>

static ddekit_lock_t lock;
static ddekit_condvar_t *cv;

// i386/i386/machdep.c:174
int cold = 1;

static void bsd_init_cold(void *arg) {
	ddekit_lock_init(&lock);
	cv = ddekit_condvar_init();
}
SYSINIT(bsd_init_cold, SI_DDE_COLD, SI_ORDER_FIRST, bsd_init_cold, NULL);

void bsd_unset_cold() {
	ddekit_lock_lock(&lock);
	cold = 0;
	ddekit_condvar_broadcast(cv);
	ddekit_lock_unlock(&lock);
}

void bsd_wait_while_cold() {
	ddekit_lock_lock(&lock);
	while (cold) {
		ddekit_condvar_wait(cv, &lock);
	}
	ddekit_lock_unlock(&lock);
}
