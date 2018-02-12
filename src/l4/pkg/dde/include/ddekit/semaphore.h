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

/** \defgroup DDEKit_synchronization */

struct ddekit_sem;
typedef struct ddekit_sem ddekit_sem_t;

/** Initialize DDEKit semaphore.
 *
 * \ingroup DDEKit_synchronization
 *
 * \param value  initial semaphore counter
 */
ddekit_sem_t *ddekit_sem_init(int value);

/** Uninitialize semaphore.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_sem_deinit(ddekit_sem_t *sem);

/** Semaphore down method. */
void ddekit_sem_down(ddekit_sem_t *sem);

/** Semaphore down method, non-blocking.
 *
 * \ingroup DDEKit_synchronization
 *
 * \return 0   success
 * \return !=0 would block
 */
int  ddekit_sem_down_try(ddekit_sem_t *sem);

/** Semaphore down with timeout.
 *
 * \ingroup DDEKit_synchronization
 *
 * \return 0   success
 * \return !=0 timed out
 */
int  ddekit_sem_down_timed(ddekit_sem_t *sem, int timo);

/** Semaphore up method. 
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_sem_up(ddekit_sem_t *sem);

EXTERN_C_END
