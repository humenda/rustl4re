#include <l4/util/util.h>
#include <l4/util/atomic.h>
#include <l4/lib_vt100/vt100.h>

void vt100_semaphore_init(vt100_semaphore_t *sem, int val)
{
  sem_init(sem, 0, val);
}

void vt100_semaphore_down(vt100_semaphore_t *sem)
{
  sem_wait(sem);
}

int vt100_semaphore_try_down(vt100_semaphore_t *sem)
{
  return !sem_trywait(sem);
}

void vt100_semaphore_up(vt100_semaphore_t *sem)
{
  sem_post(sem);
}

void vt100_sleep(int ms)
{
  l4_sleep(ms);
}

void vt100_dec32(unsigned *val) { l4util_dec32(val); }
void vt100_inc32(unsigned *val) { l4util_inc32(val); }

int _DEBUG = 0;
