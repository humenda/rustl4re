#include <errno.h>
#include <pthread.h>
#include "internals.h"
#include <bits/kernel-features.h>

#include "pt_rep.h"

int
__pthread_spin_lock (pthread_spinlock_t *lock)
{
  if (*lock == 0) {
    *lock = rep_find_free_lock_entry((void*)lock);
  }
  rep_acquire(*lock);
  rep_function_reset_regs();
  return 0;
}
weak_alias (__pthread_spin_lock, pthread_spin_lock)


int
__pthread_spin_trylock (pthread_spinlock_t *lock)
{
  enter_kdebug("spin_try_lock");
  return EBUSY;
}
weak_alias (__pthread_spin_trylock, pthread_spin_trylock)


int
__pthread_spin_unlock (pthread_spinlock_t *lock)
{
  rep_release(*lock);
  rep_function_reset_regs();
  return 0;
}
weak_alias (__pthread_spin_unlock, pthread_spin_unlock)

int
__pthread_spin_init (pthread_spinlock_t *lock, int pshared)
{
  *lock = 0;
  rep_function_reset_regs();
  return 0;
}
weak_alias (__pthread_spin_init, pthread_spin_init)


int
__pthread_spin_destroy (pthread_spinlock_t *lock)
{
  /* Nothing to do.  */
  return 0;
}
weak_alias (__pthread_spin_destroy, pthread_spin_destroy)

