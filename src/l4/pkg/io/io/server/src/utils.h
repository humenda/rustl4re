#pragma once

#include <pthread.h>

/**
 * RAII Lock guard for pthread_mutex_t.
 */
class Pthread_mutex_guard
{
public:
  Pthread_mutex_guard(pthread_mutex_t *mutex) : _m(mutex)
  { pthread_mutex_lock(_m); }

  ~Pthread_mutex_guard()
  { pthread_mutex_unlock(_m); }

private:
  pthread_mutex_t *_m = 0;
};

template<typename COND>
bool
poll_loop(unsigned count, COND &&cond)
{
  while (cond())
    {
      if (count == 0)
        return false;

      --count;
    }
  return true;
}
