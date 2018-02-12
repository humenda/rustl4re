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

#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/assert.h>

#include <pthread-l4.h> // pthread_l4_cap
#include <bits/pthreadtypes.h> // mutex->__m_owner

#include "internals.h"

#define DDEKIT_DEBUG_LOCKS 1

void ddekit_lock_init    (ddekit_lock_t *mtx)
{
	*mtx = ddekit_simple_malloc(sizeof(struct ddekit_lock));
	Assert(pthread_mutex_init(&(*mtx)->mtx, NULL) == 0);
}


void ddekit_lock_deinit  (ddekit_lock_t *mtx)
{
	pthread_mutex_destroy(&(*mtx)->mtx);
	ddekit_simple_free(*mtx);
}


void ddekit_lock_lock    (ddekit_lock_t *mtx)
{
	int ret = pthread_mutex_lock(&(*mtx)->mtx);
	Assert(ret == 0);
}


int  ddekit_lock_try_lock(ddekit_lock_t *mtx)
{
	return pthread_mutex_trylock(&(*mtx)->mtx);
}


void ddekit_lock_unlock  (ddekit_lock_t *mtx)
{
	int ret = pthread_mutex_unlock(&(*mtx)->mtx);
	Assert(ret == 0);
}


int ddekit_lock_owner(ddekit_lock_t *mtx)
{
	return (int)((*mtx)->mtx.__m_owner);
}


int ddekit_lock_is_locked(ddekit_lock_t *mtx)
{
  return (*mtx)->mtx.__m_lock.__status;
}
