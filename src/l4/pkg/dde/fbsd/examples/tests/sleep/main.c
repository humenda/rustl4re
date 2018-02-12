#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <l4/dde/dde.h>
#include <l4/sys/types.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

struct mtx mtx1;

static void slpr(void *foo) {
	bsd_dde_prepare_thread("slpr");

	mtx_lock(&mtx1);
	printf("sleeping on %p\n", foo);
	msleep(foo, &mtx1, 0, "the foo argument", 0);
	printf("woken up on %p\n", foo);
	mtx_unlock(&mtx1);
}


int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	bzero(&mtx1, sizeof mtx1);
	mtx_init(&mtx1, "dummy mutex", NULL, MTX_DEF);

	l4thread_create(&slpr, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_create(&slpr, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(100);
	printf("calling wakeup\n");
	wakeup((void*)1);

	// call wakeup with no waiters
	l4thread_sleep(100);
	printf("calling wakeup\n");
	wakeup((void*)1);

	l4thread_sleep_forever();
	return 0;
}
