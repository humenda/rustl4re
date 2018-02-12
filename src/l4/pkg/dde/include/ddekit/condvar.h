#pragma once

/** \file ddekit/condvar.h */

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
#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

struct ddekit_condvar;
typedef struct ddekit_condvar ddekit_condvar_t;

/** Initialize conditional variable.
 *
 * \ingroup DDEKit_synchronization
 */
ddekit_condvar_t * ddekit_condvar_init(void);

/** Uninitialize conditional variable. 
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_condvar_deinit(ddekit_condvar_t *cvp);

/** Wait on a conditional variable.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_condvar_wait(ddekit_condvar_t *cvp, ddekit_lock_t *mp);

/** Wait on a conditional variable at most until a timeout expires.
 *
 * \ingroup DDEKit_synchronization
 *
 * \param cvp    pointer to condvar
 * \param mp     lock
 * \param timo   timeout in ms
 *
 * \return 0     success
 * \return !=0   timeout
 */
int  ddekit_condvar_wait_timed(ddekit_condvar_t *cvp, ddekit_lock_t *mp, int timo);

/** Send signal to the next one waiting for condvar.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_condvar_signal(ddekit_condvar_t *cvp);

/** Send signal to all threads waiting for condvar.
 *
 * \ingroup DDEKit_synchronization
 */
void ddekit_condvar_broadcast(ddekit_condvar_t *cvp);

EXTERN_C_END
