#include <sys/taskqueue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <l4/dde/dde.h>
#include <l4/sys/types.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

MALLOC_DEFINE(M_TST, "test", "test");

static void tqfn(void *arg, int pending) {
	printf("arg=%d pending=%d\n", (int) arg, pending);
	l4thread_sleep(200);
}

int main(int argc, char **argv) {
	int i;
	int prio=0;
	struct task *tqtask; 
	struct taskqueue *tq;

	ddekit_init();
	bsd_dde_init();

	tq = taskqueue_thread;
//	tq = taskqueue_swi;

	tqtask = malloc(sizeof(*tqtask), M_TST, M_ZERO|M_WAITOK);
	bzero(tqtask, sizeof(*tqtask));
	TASK_INIT(tqtask, prio, tqfn, (void*) prio);
	printf("enqueueing 10 tasks, step by step\n");
	for (i=1; i<=10; i++) {
		taskqueue_enqueue(tq, tqtask);
		l4thread_sleep(35);
	}

	l4thread_sleep(2000);
	printf("all enqueued tasks should be done now. testing now with prio's\n");

	for (i=1; i<=10; i++) {
		prio = (2*(i%2)-1) * i + 10;
		printf("enqueueing task with prio=%d\n", prio);

		tqtask = malloc(sizeof(*tqtask), M_TST, M_ZERO|M_WAITOK);
		bzero(tqtask, sizeof(*tqtask));
		TASK_INIT(tqtask, prio, tqfn, (void*) prio);

		taskqueue_enqueue(tq, tqtask);
		l4thread_sleep(35);
	}

	l4thread_sleep_forever();
	return 0;
}
