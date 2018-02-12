#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/sched.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dde_fbsd/thread.h>

#define dbg_this 1

static ddekit_lock_t sched_ddelock;

/**
 * Change a thread's priority.
 */
void sched_prio(struct thread *td, u_char prio) {
	if (dbg_this) {
		if (td->td_priority!=prio)
			printf("thread=\"%s\" old_prio=%d new_prio=%d caller=\"%s\"\n",
					ddekit_thread_get_name(td->dde_thread), td->td_priority, prio,
					ddekit_thread_get_name(curthread->dde_thread)
			);
	}
	// adjust dde thread priority
	//XXXY
	// update bsd thread structure
	td->td_priority = prio;
}

/**
 * Inform the scheduler that the thread is going to sleep now.
 * Called before sched_switch.
 */
void sched_sleep(struct thread *td) {
	// nothing to do
}

/**
 * Inform the scheduler that the thread is woken up now.
 */
void sched_wakeup(struct thread *td) {
	mtx_assert(&sched_lock, MA_OWNED);

	ddekit_lock_lock(&sched_ddelock);
	if (! TD_IS_INHIBITED(td)) {
		ddekit_thread_wakeup(td->dde_thread);
	} else {
		panic("sched_wakeup but td_inhibitors=%d!=0", td->td_inhibitors);
	}
	ddekit_lock_unlock(&sched_ddelock);
}

/**
 * Switch to the given thread. This oftenly means curthread
 * has changed its state to sleeping.
 */
void sched_switch(struct thread *td, struct thread *newtd, int flags) {
	mtx_assert(&sched_lock, MA_OWNED);

	// prepare for sleep
	ddekit_lock_lock(&sched_ddelock);
	mtx_unlock_spin(&sched_lock);

	if (TD_IS_INHIBITED(td)) {
		// sleep
		ddekit_thread_sleep(&sched_ddelock);
		// do some clean up
		if (TD_CAN_RUN(td))
			TD_SET_RUNNING(td);
	}

	// we woke up! unprepare now
	ddekit_lock_unlock(&sched_ddelock);
	mtx_lock_spin(&sched_lock);
}

int sched_load(void) {
	return 1;
}

void setrunqueue(struct thread *td, int flags) {
	// let sched_wakeup examine the circumstances
	sched_wakeup(td);
}

struct thread *thread_switchout(struct thread *td, int flags, struct thread *nextthread) {
	return nextthread;
}

static void bsd_sched_init(void *unused) {
	ddekit_lock_init(&sched_ddelock);
}
SYSINIT(bsd_sched, SI_DDE_SCHED, SI_ORDER_FIRST, bsd_sched_init, NULL);
