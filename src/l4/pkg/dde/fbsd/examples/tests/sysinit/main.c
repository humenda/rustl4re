#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>

#include <l4/dde/dde.h>
#include <l4/sys/types.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static void sysinitfun(void *data) {
	printf("sysinitfun(data=%p)\n", data);
}

SYSINIT(sitest_2nd, SI_SUB_LOCK, SI_ORDER_SECOND, sysinitfun, SI_ORDER_SECOND);
SYSINIT(sitest_1st, SI_SUB_LOCK, SI_ORDER_FIRST, sysinitfun, SI_ORDER_FIRST);
SYSINIT(sitest_mid, SI_SUB_LOCK, SI_ORDER_MIDDLE, sysinitfun, SI_ORDER_MIDDLE);

int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	l4thread_sleep_forever();
	return 0;
}
