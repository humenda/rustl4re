#include <sys/types.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <machine/stdarg.h>

#include <l4/dde/dde.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static struct sx sx1;

static void shar(void *data) {
	LOG("%d: entered", (int) data);
	bsd_dde_prepare_thread("shar");

	LOG("%d: locking shared...", (int) data);
	sx_slock(&sx1);
	LOG("%d: locking shared... successful", (int) data);
	sx_sunlock(&sx1);
}

static void excl(void *data) {
	LOG("%d: entered", (int) data);
	bsd_dde_prepare_thread("excl");

	LOG("%d: locking exclusive...", (int) data);
	sx_xlock(&sx1);
	LOG("%d: locking exclusive... successful", (int) data);
	sx_xunlock(&sx1);
}

int main(int argc, char **argv) {
	LOG_Enter("");

	ddekit_init();
	bsd_dde_init();

	sx_init(&sx1, "test sx lock");

	LOG("----------------------------------------");

	LOG("0: locking exclusive");
	sx_xlock(&sx1);
	l4thread_create(excl, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(100);
	LOG("0: unlocking exclusive");
	sx_xunlock(&sx1);
	l4thread_sleep(100);

	LOG("----------------------------------------");

	LOG("0: locking exclusive");
	sx_xlock(&sx1);
	l4thread_create(shar, (void*) 2, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(100);
	LOG("0: unlocking exclusive");
	sx_xunlock(&sx1);
	l4thread_sleep(100);

	LOG("----------------------------------------");

	LOG("0: locking shared");
	sx_slock(&sx1);
	l4thread_create(excl, (void*) 3, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(100);
	LOG("0: unlocking shared");
	sx_sunlock(&sx1);
	l4thread_sleep(100);

	LOG("----------------------------------------");

	LOG("0: locking shared");
	sx_slock(&sx1);
	l4thread_create(shar, (void*) 4, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(100);
	LOG("0: unlocking shared");
	sx_sunlock(&sx1);
	l4thread_sleep(100);

	LOG("----------------------------------------");

	l4thread_sleep_forever();
	return 0;
}
