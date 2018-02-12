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

#include "dopestd.h"
#include "thread.h"
#include <pthread-l4.h>

//struct thread {
//	pthread_t tid;
//};

//struct mutex {
////	pthread_mutex_t sem;
//	s8 locked_flag;
//};


int init_thread(struct dope_services *d);


#if 0
static unsigned long hex2u32(const char *s) {
	int i;
	unsigned long result=0;
	for (i=0;i<8;i++, s++) {
		if (!(*s)) return result;
		result = result*16 + (*s & 0xf);
		if (*s > '9') result += 9;
	}
	return result;
}
#endif


/*********************/
/* SERVICE FUNCTIONS */
/*********************/

/*** ALLOCATE THREAD STRUCTURE ***/
static THREAD *alloc_thread(void) {
	THREAD *new = (THREAD *)zalloc(sizeof(THREAD));
	return new;
}


/*** FREE THREAD STRUCTURE ***/
static void free_thread(THREAD *t) {
	if (!t) return;
	free(t);
}


/*** COPY CONTEXT OF A THREAD ***/
static void copy_thread(THREAD *src, THREAD *dst) {
	if (!src || !dst) return;
	dst->tid = src->tid;
}


/*** CREATE NEW THREAD AND RETURN THREAD IDENTIFIER ***
 *
 * \param dst_tid   out parameter to where the new thread id should be stored
 * \param entry     start function of new thread
 * \param arg       private argument to be passed to thread start function
 * \return          0 on success, otherwise a negative error code
 *
 * The dst_tid parameter can be NULL.
 */
static int start_thread(THREAD *dst_tid, void *(*entry)(void *), void *arg)
{
  (void)arg;
  pthread_t x;

  if (pthread_create(&x, NULL, entry, NULL))
    return -1;

  INFO(printf("Thread(create): new thread created\n"));

  /* return thread id to out parameter */
  if (dst_tid)
    dst_tid->tid = x;

  return 0;
}


/*** KILL THREAD ***/
static void kill_thread(THREAD *tid)
{
  pthread_cancel(tid->tid);
}


/*** CREATE NEW MUTEX AND SET IT UNLOCKED ***/
static MUTEX *create_mutex(int init)
{
  MUTEX *result;


  result = malloc(sizeof(MUTEX));
  if (!result) {
    INFO(printf("Thread(create_mutex): out of memory!\n");)
      return NULL;
  }

  pthread_mutex_init(&result->sem, NULL);

  if (init) {
    pthread_mutex_lock(&result->sem);
    result->locked_flag = 1;
  } else
    result->locked_flag = 0;
  return result;
}


/*** DESTROY MUTEX ***/
static void destroy_mutex(MUTEX *m)
{
  if (!m)
    return;
  pthread_mutex_destroy(&m->sem);
  free(m);
}


/*** LOCK MUTEX ***/
static void mutex_down(MUTEX *m)
{
  if (!m)
    return;

  pthread_mutex_lock(&m->sem);
  m->locked_flag = 1;
}


/*** UNLOCK MUTEX ***/
static void mutex_up(MUTEX *m)
{
  if (!m)
    return;

  pthread_mutex_unlock(&m->sem);
  m->locked_flag = 0;
}


/*** TEST IF MUTEX IS LOCKED ***/
static s8 mutex_is_down(MUTEX *m) {
	if (!m) return 0;
	return m->locked_flag;
}

#include <l4/sys/kdebug.h>
/*** CONVERT IDENTIFIER TO THREAD ID ***/
static int ident2thread(const char *ident, THREAD *dst)
{
  (void)ident; (void)dst;
  enter_kdebug("ident2thread");
#if 0
	l4_threadid_t *tid = &dst->tid;
	int i;
	if (!ident || !tid) return -1;

	/* check if identifier string length is valid */
	for (i = 0; i < 13; i++) {
		if (!ident[i]) return -1;
	}
	tid->raw = hex2u32(ident + 7);
#endif
	return -1;
}


/*** DETERMINE IF TWO THREADS ARE IDENTICAL ***
 *
 * \return   1 if task is equal, 2 it task and thread id is equal
 */
static int thread_equal(THREAD *t1, THREAD *t2)
{
  (void)t1; (void)t2;
  printf("%s %d\n", __func__, __LINE__);
#if 0
	if (!t1 || !t2) return 0;

	if (t1->tid.id.task == t2->tid.id.task) {
		if (t1->tid.id.lthread == t2->tid.id.lthread) return 2;
		return 1;
	}
#endif
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
	mutex_down,
	mutex_up,
	mutex_is_down,
	ident2thread,
	thread_equal,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_thread(struct dope_services *d) {
	d->register_module("Thread 1.0", &services);
	return 1;
}
