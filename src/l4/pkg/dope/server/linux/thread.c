/*
 * \brief   DOpE thread handling module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * Component that provides functions to  handle
 * threads and related things (e.g. semaphores)
 * to the other components of DOpE.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

//#define THREAD SDL_Thread
//#define MUTEX  SDL_mutex

#include "SDL/SDL.h"
#include "SDL/SDL_thread.h"
#include "dopestd.h"
#include "thread.h"

struct thread {
	SDL_Thread *t;
};

struct mutex {
	SDL_mutex *m;
};

int init_thread(struct dope_services *d);

/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static THREAD *alloc_thread(void) {
	THREAD *new = (THREAD *)malloc(sizeof(THREAD));
	memset(new, 0, sizeof(THREAD));
	return new;
}


static void free_thread(THREAD *t) {
	if (t) free(t);
}


static void copy_thread(THREAD *src, THREAD *dst) {
	if (!dst || !src) return;
	dst->t = src->t;
}


static int start_thread(THREAD *dst_tid, void (*entry)(void *), void *arg) {
	SDL_Thread *new = SDL_CreateThread((int (*)(void *))entry,arg);
	if (dst_tid) dst_tid->t = new;
	return 0;
}


static void kill_thread(THREAD *tid) {
	SDL_KillThread(tid->t);
}


static MUTEX *create_mutex(int init_locked) {
	MUTEX *new = (MUTEX *)malloc(sizeof(MUTEX));
	new->m = SDL_CreateMutex();
	return new;
}

static void destroy_mutex(MUTEX *m) {
	if (!m) return;
	SDL_DestroyMutex(m->m);
	free(m);
}

static void lock_mutex(MUTEX *m) {
	SDL_mutexP(m->m);
}

static void unlock_mutex(MUTEX *m) {
	SDL_mutexV(m->m);
}

static s8 mutex_is_down(MUTEX *m) {
	ERROR(printf("Thread(thread_equal): not yet implemented\n"));
	return 0;
}

static int ident2thread(const char *ident, THREAD *t) {
	ERROR(printf("Thread(thread_equal): not yet implemented\n"));
	return 0;
}

static int thread_equal(THREAD *t1, THREAD *t2) {
	ERROR(printf("Thread(thread_equal): not yet implemented\n"));
	return 0;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct thread_services services = {
	alloc_thread,
	free_thread,
	copy_thread,
	start_thread,
	kill_thread,
	create_mutex,
	destroy_mutex,
	lock_mutex,
	unlock_mutex,
	mutex_is_down,
	ident2thread,
	thread_equal,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_thread(struct dope_services *d) {

	d->register_module("Thread 1.0",&services);
	return 1;
}
