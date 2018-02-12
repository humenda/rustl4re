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

#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

struct ddekit_lock;
typedef struct ddekit_lock *ddekit_lock_t;

/** Initialize a DDEKit lock. 
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_lock_init    (ddekit_lock_t *mtx);

L4_INLINE void ddekit_lock_init_unlocked(ddekit_lock_t *mtx);

/** Uninitialize a DDEKit lock.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_lock_deinit  (ddekit_lock_t *mtx);

/** Acquire a lock.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_lock_lock    (ddekit_lock_t *mtx);

/** Acquire a lock, non-blocking.
 *
 * \return 0 on success
 *         <0 error code
 *
 * \ingroup DDEKit_synchronization
 */
int  ddekit_lock_try_lock(ddekit_lock_t *mtx);

/** Unlock function.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_lock_unlock  (ddekit_lock_t *mtx);

/** Get lock owner.
 *
 *  \return 0 unlocked
 *          !=0 owner
 * \ingroup DDEKit_synchronization
 */
int ddekit_lock_owner(ddekit_lock_t *mtx);

/** Find out if lock is locked.
 *
 *  \return 0 unlocked
 *          !=0 owner
 * \ingroup DDEKit_synchronization
 */
int ddekit_lock_is_locked(ddekit_lock_t *mtx);

L4_INLINE void ddekit_lock_init_unlocked(ddekit_lock_t *mtx)
{
	ddekit_lock_init(mtx);
}

EXTERN_C_END
