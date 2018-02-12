#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <machine/stdarg.h>

#include <l4/dde/fbsd/dde.h>
#include <l4/dde/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static struct sema sem1;

static void cons(void *data) {
	int res;

	bsd_dde_prepare_thread("cons");

	LOG("%d: entered (value=%d)", (int) data, sema_value(&sem1));

	res = sema_timedwait(&sem1, hz/10);
	LOG("%d: %s (value=%d)", (int) data, (res?"timeout":"success"), sema_value(&sem1));
}

int main(int argc, char **argv) {
	LOG_Enter("");

	ddekit_init();
	bsd_dde_init();
	
	sema_init(&sem1, 0, "test semaphore");

	LOG("----------------------------------------");

	LOG("wait for timeout");
	l4thread_create(cons, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	LOG("----------------------------------------");

	LOG("calling post before");
	sema_post(&sem1);
	l4thread_create(cons, (void*) 2, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	LOG("----------------------------------------");

	LOG("waking up one of two threads by post");
	l4thread_create(cons, (void*) 3, L4THREAD_CREATE_ASYNC);
	l4thread_create(cons, (void*) 4, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(30);
	sema_post(&sem1);
	l4thread_sleep(200);

	LOG("----------------------------------------");

	LOG("waking up two threads by signal");
	l4thread_create(cons, (void*) 5, L4THREAD_CREATE_ASYNC);
	l4thread_create(cons, (void*) 6, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(30);
	sema_post(&sem1);
	l4thread_sleep(30);
	sema_post(&sem1);
	l4thread_sleep(200);

	LOG("----------------------------------------");

	l4thread_sleep_forever();
	return 0;
}
