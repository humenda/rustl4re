/**
 * Functions dealing with BSD thread structures.
 * Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/sleepqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resourcevar.h>

#include <dde_fbsd/thread.h>

#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/memory.h>

#include <l4/dde/fbsd/bsd_dde.h>

static MALLOC_DEFINE(M_THREAD, "threads", "thread + proc structs");

/**
 * The process to which most new threads will be connected.
 */
struct proc   proc0;
static struct ksegrp proc0_ksegrp;
static struct pstats proc0_pstats;

struct thread thread0;

static struct ddekit_slab *thread_slab;

struct proc *bsd_alloc_proc() {
	return malloc(sizeof(struct proc), M_THREAD, M_WAITOK|M_ZERO);
}

void bsd_free_proc(struct proc *p) {
	free(p, M_THREAD);
}

void bsd_init_proc(struct proc *p) {
	struct ksegrp *kg;

	if (p!=&proc0) bzero(p, sizeof(*p));
	TAILQ_INIT(&p->p_ksegrps);
	TAILQ_INIT(&p->p_threads);
	p->p_sflag |= PS_INMEM;
	p->p_state |= PRS_NORMAL;
	if (p!=&proc0) mtx_init(&p->p_mtx, "process lock", NULL, MTX_DEF | MTX_DUPOK);

	if (p==&proc0) kg = &proc0_ksegrp;
	else           kg = malloc(sizeof(*kg), M_THREAD, M_WAITOK|M_ZERO);
	TAILQ_INSERT_TAIL(&p->p_ksegrps, kg, kg_ksegrp);
	p->p_numksegrps = 1;

	if (p==&proc0) p->p_stats = &proc0_pstats;
	else           p->p_stats = malloc(sizeof(*p->p_stats), M_THREAD, M_WAITOK|M_ZERO);
}

void bsd_deinit_proc(struct proc *p) {
	struct ksegrp *kg;

	kg = TAILQ_FIRST(&p->p_ksegrps);
	if (p!=&proc0) free(kg, M_THREAD);

	if (p!=&proc0) free(p->p_stats, M_THREAD);
}

struct thread *bsd_alloc_thread(void) {
	struct thread *td;
	
	// allocating from slab because bsd_malloc assumes already assigned
	// curthread
	td = ddekit_slab_alloc(thread_slab);
	bzero(td, sizeof(*td));

	return td;
}

void bsd_free_thread(struct thread *td) {
	ddekit_slab_free(thread_slab, td);
}

void bsd_init_thread_proc(struct thread *td, struct proc *p) {
	td->td_proc = p;
	mtx_lock(&p->p_mtx);
	TAILQ_INSERT_TAIL(&(td->td_proc)->p_threads, td, td_plist);
	p->p_numthreads++;
	td->td_ksegrp = TAILQ_FIRST(&p->p_ksegrps);
	mtx_unlock(&p->p_mtx);

	td->td_state = TDS_RUNNING;
	
	if (td!=&thread0)
		td->td_sleepqueue = sleepq_alloc();
}

void bsd_init_thread(struct thread *td) {
	bsd_init_thread_proc(td, &proc0);
}

void bsd_deinit_thread(struct thread *td) {
	if (td != &thread0)
		sleepq_release(td->td_sleepqueue);
}

/**
 * assigns a bsd thread structure to the current dde thread
 */
void bsd_assign_thread(struct thread *td) {
	ddekit_thread_set_my_data(td);
	td->dde_thread = ddekit_thread_myself();
}

/**
 * convenience function allocating and initializing a bsd thread structure and
 * assigning it to the current dde thread.
 */
void bsd_thread_setup_myself() {
	struct thread *td;

	td = bsd_alloc_thread();
	bsd_assign_thread(td);
	bsd_init_thread(td);
}

/**
 * convenience function for dde apps, preparing the current thread for working in the bsd environment.
 * initializing the current thread created outside the dde first at the dde then at the bsd
 */
void bsd_dde_prepare_thread(const char *name) {
	ddekit_thread_setup_myself(name);
	bsd_thread_setup_myself();
}

struct thread *bsd_get_curthread() {
	return (struct thread *) ddekit_thread_get_my_data();
}

struct thread *bsd_get_thread(ddekit_thread_t *thread) {
	return (struct thread *) ddekit_thread_get_data(thread);
}

static void bsd_threads_init(void *arg) {
	thread_slab = ddekit_slab_init(sizeof(struct thread), 1);
}
SYSINIT(bsd_threads_init, SI_DDE_THREADS, SI_ORDER_FIRST, bsd_threads_init, NULL);

static void bsd_proc0_init(void *arg) {
	sleepinit();

	bsd_init_proc(&proc0);

	bsd_init_thread(&thread0);
}
SYSINIT(bsd_proc0_init, SI_SUB_INTRINSIC, SI_ORDER_FIRST, bsd_proc0_init, NULL);

