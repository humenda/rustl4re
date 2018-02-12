#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <l4/dde/dde.h>
#include <l4/sys/types.h>
#include <l4/thread/thread.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static struct mtx mtx1;
static struct cv cv1;

static void cond(void *data) {
	int res;

	printf("%d: entered\n", (int) data);
	bsd_dde_prepare_thread("cond");

	mtx_lock(&mtx1);
	res = cv_timedwait(&cv1, &mtx1, hz/10);
	mtx_unlock(&mtx1);
	printf("%d: %s\n", (int) data, (res?"timeout":"success"));
}

int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	printf("cold=%d\n", cold);

	mtx_init(&mtx1, "test mutex", "test", MTX_DEF);
	cv_init(&cv1, "test condvar");

	printf("----------------------------------------\n");

	printf("wait for timeout\n");
	l4thread_create(cond, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	printf("calling signal too early\n");
	cv_signal(&cv1);
	l4thread_create(cond, (void*) 2, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	printf("calling broadcast too early\n");
	cv_broadcast(&cv1);
	l4thread_create(cond, (void*) 3, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	printf("waking up one of two threads by signal\n");
	l4thread_create(cond, (void*) 4, L4THREAD_CREATE_ASYNC);
	l4thread_create(cond, (void*) 5, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(30);
	cv_signal(&cv1);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	printf("waking up two threads by signal\n");
	l4thread_create(cond, (void*) 6, L4THREAD_CREATE_ASYNC);
	l4thread_create(cond, (void*) 7, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(30);
	cv_signal(&cv1);
	l4thread_sleep(30);
	cv_signal(&cv1);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	printf("waking up two threads by broadcast\n");
	l4thread_create(cond, (void*) 8, L4THREAD_CREATE_ASYNC);
	l4thread_create(cond, (void*) 9, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(30);
	cv_broadcast(&cv1);
	l4thread_sleep(200);

	printf("----------------------------------------\n");

	l4thread_sleep_forever();
	return 0;
}
