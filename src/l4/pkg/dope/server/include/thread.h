/*
 * \brief   Interface of the thread abstraction of DOpE
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

#ifndef _DOPE_THREAD_H_
#define _DOPE_THREAD_H_

#if !defined(THREAD)
#define THREAD struct thread
#endif

#if !defined(MUTEX)
#define MUTEX struct mutex
#endif

//struct thread;

#include <pthread.h>

struct thread {
   pthread_t tid;
};

//struct mutex;

struct mutex {
   pthread_mutex_t sem;
   s8 locked_flag;
};


struct thread_services {
	THREAD *(*alloc_thread)  (void);
	void    (*free_thread)   (THREAD *);
	void    (*copy_thread)   (THREAD *src, THREAD *dst);
	int     (*start_thread)  (THREAD *dst_tid, void *(*entry)(void *), void *arg);
	void    (*kill_thread)   (THREAD *tid);
	MUTEX  *(*create_mutex)  (int init_locked);
	void    (*destroy_mutex) (MUTEX *);
	void    (*mutex_down)    (MUTEX *);
	void    (*mutex_up)      (MUTEX *);
	s8      (*mutex_is_down) (MUTEX *);
	int     (*ident2thread)  (const char *thread_ident, THREAD *dst);
	int     (*thread_equal)  (THREAD *thread, THREAD *another_thread);
};


#endif /* _DOPE_THREAD_H_ */

