#ifndef _I386_SEMAPHORE_H
#define _I386_SEMAPHORE_H

#include <linux/wait.h>
#include <pthread.h>

/* XXX Semaphores are used only for mutual exclusion in input for now. Given
 * this fact and because "kseriod" is not desirable we use L4 locks that can
 * be reentered by one and the same thread. */

struct semaphore {
	pthread_mutex_t l4_lock;
};

#define DECLARE_MUTEX(name)		\
	struct semaphore name = {PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP}

static inline void init_MUTEX (struct semaphore * sem)
{
        pthread_mutexattr_t a;
        pthread_mutexattr_init(&a);
        pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE_NP);
        pthread_mutex_init(&sem->l4_lock, &a);
        pthread_mutexattr_destroy(&a);
}

static inline void down(struct semaphore * sem)
{
	pthread_mutex_lock(&sem->l4_lock);
}

static inline void up(struct semaphore * sem)
{
	pthread_mutex_unlock(&sem->l4_lock);
}

#endif

