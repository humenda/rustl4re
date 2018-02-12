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

/**
 * Unchecked (no BSD invariants) condition variable implementation for
 * dde-internal use. Written from scratch.
 */
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/assert.h>
#include "internals.h"

struct ddekit_condvar {
	pthread_cond_t cond;
};

ddekit_condvar_t *ddekit_condvar_init()
{
	ddekit_condvar_t *c = ddekit_simple_malloc(sizeof(ddekit_condvar_t));
	int r = pthread_cond_init(&c->cond, NULL);
	Assert(r == 0);
	return c;
}


void ddekit_condvar_wait(ddekit_condvar_t *cvp, ddekit_lock_t *mp)
{
	int r = pthread_cond_wait(&cvp->cond, ddekit_lock_to_pthread(*mp));
	Assert(r == 0);
}


int ddekit_condvar_wait_timed(ddekit_condvar_t *cvp, ddekit_lock_t *mp, int timo)
{
	struct timespec abs_to = ddekit_abs_to_from_rel_ms(timo);
	return pthread_cond_timedwait(&cvp->cond,
	                              ddekit_lock_to_pthread(*mp),
	                              &abs_to);
}


void ddekit_condvar_signal(ddekit_condvar_t *cvp)
{
	int r = pthread_cond_signal(&cvp->cond);
	Assert(r == 0);
}


void ddekit_condvar_broadcast(ddekit_condvar_t *cvp)
{
	int r = pthread_cond_broadcast(&cvp->cond);
	Assert(r == 0);
}
