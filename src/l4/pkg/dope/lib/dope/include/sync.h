/*
 * \brief   DOpElib internal synchronisation interface
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _DOPELIB_SYNC_H_
#define _DOPELIB_SYNC_H_

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

struct dopelib_sem;
struct dopelib_mutex;


/*** CREATE NEW SEMAPHORE ***
 *
 * \return pointer to newly created semaphore
 */
struct dopelib_sem *dopelib_sem_create(int init_state);


/*** FREE SEMAPHORE ***/
void dopelib_sem_destroy(struct dopelib_sem *);


/*** SEMAPHORE DOWN ***
 *
 * \param sem pointer to sem which should be decremented.
 */
void dopelib_sem_wait(struct dopelib_sem *);


/*** SEMAPHORE UP ***
 *
 * \param sem pointer to sem which should be incremented
 */
void dopelib_sem_post(struct dopelib_sem *);


/*** CREATE NEW MUTEX ***
 *
 * \return pointer to newly created mutex
 */
struct dopelib_mutex *dopelib_mutex_create(int init_state);


/*** FREE MUTEX ***/
void dopelib_mutex_destroy(struct dopelib_mutex *);


/*** LOCK MUTEX ***
 *
 * \param mutex pointer to mutex which should be locked
 */
void dopelib_mutex_lock(struct dopelib_mutex *);


/*** UNLOCK MUTEX ***
 *
 * \param mutex pointer to mutex which should be unlocked
 */
void dopelib_mutex_unlock(struct dopelib_mutex *);

EXTERN_C_END

#endif /* _DOPELIB_SYNC_H_ */
