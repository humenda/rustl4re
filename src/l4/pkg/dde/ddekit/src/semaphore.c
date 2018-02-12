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

#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/assert.h>

#include <pthread-l4.h>
#include <semaphore.h>

#include "internals.h"

struct ddekit_sem
{
	sem_t sem;
};


ddekit_sem_t *ddekit_sem_init(int value)
{
	ddekit_sem_t *s = ddekit_simple_malloc(sizeof(ddekit_sem_t));
	int r = sem_init(&s->sem, 0, value);
	Assert(r == 0);

	return s;
}


void ddekit_sem_deinit(ddekit_sem_t *sem) 
{
	int r = sem_destroy(&sem->sem);
	Assert(r == 0);
	ddekit_simple_free(sem);
}


void ddekit_sem_down(ddekit_sem_t *sem)
{
	int r = sem_wait(&sem->sem);
	Assert(r == 0);
}


/* returns 0 on success, != 0 when it would block */
int  ddekit_sem_down_try(ddekit_sem_t *sem)
{
	return sem_trywait(&sem->sem);
}


/* returns 0 on success, != 0 on timeout */
int  ddekit_sem_down_timed(ddekit_sem_t *sem, int to)
{
	/*
	 * PThread semaphores want absolute timeouts, to is relative though.
	 */
	struct timespec abs_to = ddekit_abs_to_from_rel_ms(to);

	return sem_timedwait(&sem->sem, &abs_to);
}


void ddekit_sem_up(ddekit_sem_t *sem)
{
	int r = sem_post(&sem->sem);
	Assert(r == 0);
}
