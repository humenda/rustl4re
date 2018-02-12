/*
 * \brief   DOpE client library - synchronisation primitives
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2003  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** GENERAL INCLUDES ***/
#include <stdlib.h>
#include <string.h>

/*** LINUX INCLUDES ***/
#include <pthread.h>
#include <semaphore.h>

/*** LOCAL INCLUDES ***/
#include "sync.h"

struct dopelib_sem   { sem_t           sem;   };
struct dopelib_mutex { pthread_mutex_t mutex; };


struct dopelib_sem *dopelib_sem_create(int init_state) {
	struct dopelib_sem *new_sem = malloc(sizeof(struct dopelib_sem));
	if (!new_sem) return NULL;
	memset(new_sem, 0, sizeof(struct dopelib_sem));
	sem_init(&new_sem->sem, 0, !init_state);
	return new_sem;
}


void dopelib_sem_destroy(struct dopelib_sem *s) {
	if (!s) return;
	sem_destroy(&s->sem);
	free(s);
}


void dopelib_sem_wait(struct dopelib_sem *s) {
	if (s) sem_wait(&s->sem);
}


void dopelib_sem_post(struct dopelib_sem *s) {
	if (s) sem_post(&s->sem);
}


struct dopelib_mutex *dopelib_mutex_create(int init_state) {
	struct dopelib_mutex *new_mutex = malloc(sizeof(struct dopelib_mutex));
	if (!new_mutex) return NULL;
	memset(new_mutex, 0, sizeof(struct dopelib_mutex));
	if (init_state) dopelib_mutex_lock(new_mutex);
	return new_mutex;
}


void dopelib_mutex_destroy(struct dopelib_mutex *m) {
	if (m) free(m);
}


void dopelib_mutex_lock(struct dopelib_mutex *m) {
	if (m) pthread_mutex_lock(&m->mutex);
}


void dopelib_mutex_unlock(struct dopelib_mutex *m) {
	if (m) pthread_mutex_unlock(&m->mutex);
}

