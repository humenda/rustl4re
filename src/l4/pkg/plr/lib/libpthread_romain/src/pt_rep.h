#include "descr.h"

#include <l4/util/atomic.h>
#include <l4/plr/pthread_rep.h>
#include <l4/sys/kdebug.h>

static lock_info * const __lip_address = (lock_info* const)LOCK_INFO_ADDR;

#define YIELD()  yield() 
#define BARRIER() asm volatile ("" : : : "memory");

#define GO_TO_SLEEP 0

/*
 * When executing _rep code, we access data shared across all replicas.
 * This means, replicas may read differing values here and these values
 * may end up in caller-saved registers (EBX, ECX, EDX). In turn, the
 * next replica exception may produce differing results if the compiler
 * did not actually use those and therefore did not save regs before
 * calling into pthread_*().
 * 
 * To accomodate for that, we hard-reset the caller-saved registers to
 * 0 here. This will enforce identical values and does not hurt real
 * caller-saving, because then the caller code will restore these regs
 * after we return to it.
 */
static inline void rep_function_reset_regs(void)
{
  BARRIER();
  asm volatile ("mov $0, %%ebx\t\n"
                "mov $0, %%ecx\t\n"
                "mov $0, %%edx\t\n"
                ::: "memory");
  BARRIER();
}

#ifdef PT_SOLO
#define yield stall
#else
static inline void yield(void)
{
	// XXX: Really need to reset regs here? -> we will never
	//      end up in the master here anyway.
	rep_function_reset_regs();
	asm volatile ("ud2" : : : "edx", "ecx", "ebx", "memory");
}
#endif


static inline void lock_rep_wait(pthread_mutex_t* mutex)
{
    rep_function_reset_regs();
    asm volatile (
                  "push %0\t\n"
                  "mov $0xA020, %%eax\t\n"
                  "call *%%eax\t\n"
                  "pop %0\t\n"
                  :
                  : "r" (mutex)
                  : "eax", "memory");
}



static inline void lock_rep_post(pthread_mutex_t* mutex)
{
    /*
     * Send the actual notification. This is a special case in the master,
     * because here only one replica performs the system call while all
     * others continue untouched.
     */
	rep_function_reset_regs();
    asm volatile ("push %0\t\n"
                  "mov $0xA040, %%eax\t\n"
                  "call *%%eax\t\n"
                  "pop %0\t\n": : "r" (mutex) : "eax", "memory");
}


static unsigned
rep_find_free_lock_entry(void *sync_ptr)
{
  unsigned i = 0;
  lock_info* li = get_lock_info();
  
  /*
   * find either the respective lock (if it has been registered by another
   * replica yet) or a free slot to use
   */
  for (i = 1; i < NUM_LOCKS; ++i) {

	lock_li(li, i);

	// slot already acquired by another replica
	if (li->locks[i].lockdesc == (l4_addr_t)sync_ptr) {
	  unlock_li(li, i);
	  break;
	}

	// free slot -> we can use it
	if (li->locks[i].owner == lock_entry_free) {
	  li->locks[i].lockdesc = (l4_addr_t)sync_ptr;
	  li->locks[i].owner    = lock_unowned;
	  unlock_li(li, i);
	  break;
	}

	unlock_li(li, i);
  }

  // won't get out of here...
  while (i >= NUM_LOCKS) {
	  enter_kdebug("out of locks");
  }
  
  return i;
}


static void
rep_acquire(unsigned idx)
{
  lock_info* li            = get_lock_info();
  thread_self()->p_epoch  += 1;
  
  // CANARY CHECK - no locking required as this value should never change
  while (LOCK_ELEM(li, idx).canary != LIP_CANARY)
    enter_kdebug("CANARY FAILURE");
  
  /*outstring("lock() "); outhex32(thread_self()->p_epoch); outstring("\n");*/

  while (1) {
	ACQ(li, idx);
  
	/* Unowned -> I'm going to grab this lock */
    if (LOCK_ELEM(li, idx).owner == lock_unowned)
    {
      LOCK_ELEM(li, idx).owner       = (l4_addr_t)thread_self();
      LOCK_ELEM(li, idx).owner_epoch = thread_self()->p_epoch;
      LOCK_ELEM(li, idx).acq_count   = li->replica_count;
      break;
    }
    /* Owned by myself ... ? */
    else if (LOCK_ELEM(li, idx).owner == (l4_addr_t)thread_self())
    {
	  /* Only grab if I'm in the right epoch */
      if (LOCK_ELEM(li, idx).owner_epoch != thread_self()->p_epoch) {
        REL(li, idx);
        YIELD();
        continue;
      }

      break;
    }
    else /* Owned by someone else -> yield and retry. */
    {
      REL(li, idx);
      YIELD();
      continue;
    }
  }

  l4_uint16_t bits = (1 << get_replica_number());
  LOCK_ELEM(li, idx).acq_bitmap |= bits;

  REL(li, idx);
}


static void
rep_release(unsigned idx)
{
  lock_info *li = get_lock_info();
  
  ACQ(li, idx);
  
  LOCK_ELEM(li, idx).acq_count -= 1;
  if (LOCK_ELEM(li, idx).acq_count == 0) {
      LOCK_ELEM(li, idx).owner = lock_unowned;
  }

  l4_uint16_t bits = (1 << get_replica_number());
  LOCK_ELEM(li, idx).acq_bitmap &= ~bits;

  REL(li, idx);
}
