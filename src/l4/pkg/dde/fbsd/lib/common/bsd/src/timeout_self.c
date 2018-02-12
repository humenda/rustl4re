/**
 * Self-written implementation of the callout interface.
 * Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/lock.h>

#include <dde_fbsd/thread.h>
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/panic.h>

static ddekit_condvar_t *timeout_cv;
static ddekit_lock_t    timeout_mtx;

static TAILQ_HEAD(, callout) to_list = TAILQ_HEAD_INITIALIZER(to_list);

void callout_init(struct callout *co, int mpsafe) {
	bzero(co, sizeof *co);
	if (mpsafe)
		co->c_flags |= CALLOUT_MPSAFE;
}

void callout_reset(struct callout *co, int rel_to, void (*fun)(void *), void *arg) {
	struct callout *item;

	ddekit_lock_lock(&timeout_mtx);

	// initialize co fields
	co->c_time = rel_to + ticks;
	co->c_arg  = arg;
	co->c_func = fun;

	// remove from list in case
	if (co->c_flags & CALLOUT_PENDING) {
		TAILQ_REMOVE(&to_list, co, c_links.tqe);
	} else {
		co->c_flags |= CALLOUT_PENDING;
	}

	// insert in right order
	TAILQ_FOREACH(item, &to_list, c_links.tqe) {
		if (item->c_time > co->c_time) {
			TAILQ_INSERT_BEFORE(item, co, c_links.tqe);
			break;
		}
	}
	if (!item)
		TAILQ_INSERT_TAIL(&to_list, co, c_links.tqe);

	// wakeup timeout_loop if we have a new first item
	if (TAILQ_FIRST(&to_list)==co) {
		ddekit_condvar_signal(timeout_cv);
	}

	ddekit_lock_unlock(&timeout_mtx);
}

int _callout_stop_safe(struct callout *co, int safe) {
	int do_wakeup;

	ddekit_lock_lock(&timeout_mtx);

	if (co->c_flags & CALLOUT_PENDING) {
		do_wakeup = (TAILQ_FIRST(&to_list)==co);

		TAILQ_REMOVE(&to_list, co, c_links.tqe);
		co->c_flags &= ~CALLOUT_PENDING;

		if (do_wakeup)
			ddekit_condvar_signal(timeout_cv);
	}

	ddekit_lock_unlock(&timeout_mtx);
	return 1; // return 0 on error
}

static void timeout_loop(void* arg) {
	int timeout;
	struct callout *next;
	struct callout tocall;

	bsd_assign_thread((struct thread *)arg);

	ddekit_lock_lock(&timeout_mtx);
	while (1) {

		next = TAILQ_FIRST(&to_list);
		if (next) {
			if (next->c_time <= ticks) {
				next->c_flags &= ~CALLOUT_PENDING;
				TAILQ_REMOVE(&to_list, next, c_links.tqe);

				tocall.c_func = next->c_func;
				tocall.c_arg  = next->c_arg;
				ddekit_lock_unlock(&timeout_mtx);
				tocall.c_func(tocall.c_arg);
				ddekit_lock_lock(&timeout_mtx);

				next = TAILQ_FIRST(&to_list);
			}
		}

		if (!next) {
			ddekit_condvar_wait(timeout_cv, &timeout_mtx);
		} else {
			timeout = next->c_time - ticks;
			// cv_timedwait wants msecs as timeout param, not ticks
			if (timeout > 0)
				ddekit_condvar_wait_timed(timeout_cv, &timeout_mtx, timeout*1000/hz);
		}
		
	}
	ddekit_lock_lock(&timeout_mtx);
}

void softclock(void *dummy) {
	ddekit_debug("should never happen");
}

static void init_timeout_loop(void *foo __unused) {
	struct thread *td;

	ddekit_lock_init(&timeout_mtx);
	
	timeout_cv = ddekit_condvar_init();
	TAILQ_INIT(&to_list);

	td = bsd_alloc_thread();
	bsd_init_thread(td);
	
	ddekit_thread_create(&timeout_loop, td, "callout");
}
SYSINIT(timeout_loop, SI_DDE_TIMEOUT, SI_ORDER_FIRST, &init_timeout_loop, NULL);

