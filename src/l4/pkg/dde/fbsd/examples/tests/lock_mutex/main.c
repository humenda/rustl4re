#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <l4/dde/dde.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static struct mtx m;

static void mdwn(void *data) {
	bsd_dde_prepare_thread("mdwn");

	printf("(%d) locking\n", (int) data);

	mtx_lock(&m);
	printf("(%d) locked, unlocking\n", (int) data);
	mtx_unlock(&m);
}

int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	printf("- uninitialized, mtx_initialized= %d\n", mtx_initialized(&m));

	mtx_init(&m, "test mutex", "test", MTX_DEF|MTX_RECURSE);
	printf("- initialized,   mtx_initialized= %d\n", mtx_initialized(&m));
	printf("                 mtx_name       =\"%s\"\n", mtx_name(&m));
	printf("                 mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 1, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_lock(&m);
	printf("- locked         mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 2, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_lock(&m);
	printf("- locked x2      mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 3, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_lock(&m);
	printf("- locked x3      mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 4, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_unlock(&m);
	printf("- unlocked x1    mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 5, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_unlock(&m);
	printf("- unlocked x2    mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 6, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_unlock(&m);
	printf("- unlocked x3    mtx_owned      = %d\n", mtx_owned(&m));
	printf("                 mtx_recursed   = %d\n", mtx_recursed(&m));

	l4thread_create(mdwn, (void*) 7, L4THREAD_CREATE_ASYNC);
	l4thread_sleep(200);

	mtx_destroy(&m);
	printf("- destroyed,     mtx_initialized= %d\n", mtx_initialized(&m));

	l4thread_sleep_forever();
	return 0;
}
