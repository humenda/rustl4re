/**
 * Test for callouts (=timeouts).
 * A bit dirty - assumes hz=100.
 */
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

static void cofn(void *foo __unused) {
	printf("cofn() ticks=%d\n", ticks);
}

struct callout co;

int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	callout_init(&co, 1);

	printf("ticks=%d, to in 10 ticks\n", ticks);
	callout_reset(&co, 10, &cofn, NULL);
	l4thread_sleep(200);

	printf("ticks=%d, to in 10 ticks, should be drained before\n", ticks);
	callout_reset(&co, 10, &cofn, NULL);
	l4thread_sleep(50);
	callout_drain(&co);
	l4thread_sleep(200);

	printf("ticks=%d, to in 10 ticks, should be stopped before\n", ticks);
	callout_reset(&co, 10, &cofn, NULL);
	l4thread_sleep(50);
	callout_stop(&co);
	l4thread_sleep(200);

	printf("ticks=%d, to in 10 ticks, should be reconfigured before\n", ticks);
	callout_reset(&co, 10, &cofn, NULL);
	l4thread_sleep(50);
	printf("ticks=%d, to in 10 ticks from now (reconfigured)\n", ticks);
	callout_reset(&co, 10, &cofn, NULL);
	l4thread_sleep(200);

	l4thread_sleep_forever();
	return 0;
}
