/**
 * kthread_create() and kthread_exit() implementation. Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <machine/stdarg.h>
#include <sys/unistd.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <dde_fbsd/thread.h>
#include <dde_fbsd/cold.h>
#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/panic.h>

#define dbg_this 0

static MALLOC_DEFINE(M_KTHREAD, "kthreads", "kthreads info structures");

typedef struct newthread {
	struct thread *td;
	u_char       td_base_pri;
	u_char       td_priority;
	void       (*func)(void *);
	void        *arg;
	int          flags;
	ddekit_sem_t  *started;
} newthread_t;

/**
 * Helper for kthread_create.
 * Does some initialization which can only be done inside the newly created thread.
 * Starts the threads function at last.
 */
static void install_thread(void *arg) {
	newthread_t *ntp;
	struct thread *td;
	void (*tfun)(void *);
	void  *targ;
	int flags;

	ntp = (newthread_t *) arg;

	bsd_assign_thread(ntp->td);

	td = curthread;
	td->td_base_pri = ntp->td_base_pri;
	td->td_priority = ntp->td_priority;
	
	// save needed data
	tfun  = ntp->func;
	targ  = ntp->arg;
	flags = ntp->flags;

	// inform of startup
	// NOTE: do not access ntp afterward since it may be freed by caller!!!
	ddekit_sem_up(ntp->started);

	if (flags & RFSTOPPED) {
		mtx_lock_spin(&sched_lock);
		// set inhibited but give no reason
		TD_SET_INHIB(td, 0);
		// call sched_sleep() pro forma
		sched_sleep(td);
		// go to sleep
		mi_switch(SW_INVOL, NULL);
		mtx_unlock_spin(&sched_lock);
	}

	bsd_wait_while_cold();

	// call new kernel func
	tfun(targ);

	// XXXY: free kthread resources on thread exit
	//kthread_exit(0);
}

int kthread_create(void (*func)(void *), void *arg, struct proc **newpp,
	int flags, int pages, const char *fmt, ...)
{
	struct proc *p2;
	newthread_t *ntp;
	va_list ap;
	ddekit_thread_t *tid;
	
	p2 = bsd_alloc_proc();
	bsd_init_proc(p2);

	if (newpp) {
		*newpp = p2;
	}

	va_start(ap, fmt);
	vsnprintf(p2->p_comm, sizeof(p2->p_comm), fmt, ap);
	va_end(ap);

	if (dbg_this) {
		printf("name=\"%s\" flags=0x%b\n",
			p2->p_comm,
			flags,
			"\020"
			"\003RFFDG"
			"\005RFPROC"
			"\006RFMEM"
			"\007RFNOWAIT"
			"\015RFCFDG"
			"\016RFTHREAD"
			"\017RFSIGSHARE"
			"\021RFLINUXTHPN"
			"\022RFSTOPPED"
			"\023RFHIGHPID"
			"\040RFPPWAIT"
		);
	}
	if (flags & ~(RFFDG|RFMEM|RFNOWAIT|RFCFDG|RFHIGHPID|RFSTOPPED)) {
		ddekit_debug("i think i don't support all given flags!");
	}

	ntp = malloc(sizeof(*ntp), M_KTHREAD, M_WAITOK|M_ZERO);
	ntp->td        = bsd_alloc_thread();
	ntp->td_base_pri = curthread->td_base_pri;
	ntp->td_priority = curthread->td_priority;
	ntp->flags     = flags;
	ntp->func      = func;
	ntp->arg       = arg;
	ntp->started   = ddekit_sem_init(0);

	bsd_init_thread_proc(ntp->td, p2);

	tid = ddekit_thread_create(&install_thread, ntp, p2->p_comm);

	// wait for thread initialization
	ddekit_sem_down(ntp->started);

	// clean up
	ddekit_sem_deinit(ntp->started);
	free(ntp, M_KTHREAD);

	return 0;
}

void kthread_exit(int ecode) {
	struct thread *td;

	td = curthread;

	printf("thread exit: name=\"%s\" exit code=%d\n", ddekit_thread_get_name(td->dde_thread), ecode);

	bsd_deinit_thread(td);
	bsd_deinit_proc(td->td_proc);
	bsd_free_proc(td->td_proc);
	bsd_free_thread(td);
	ddekit_thread_exit();
}
