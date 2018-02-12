#include <sys/types.h>
#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/kthread.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

#include <l4/dde/dde.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>
#include <l4/thread/thread.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

static void kfun(void *arg) {
	LOG("arg=%p command=\"%s\"", arg, curthread->td_proc->p_comm);
	l4thread_sleep(500);
}

int main(int argc, char **argv) {
	int i;

	LOG_Enter("");

	ddekit_init();
	bsd_dde_init();
	
	for (i=1; i<=10; i++) {
		kthread_create(&kfun, (void*) i, NULL, RFMEM|RFCFDG, 0, "test kthread %d", i);
		l4thread_sleep(200);
	}

	l4thread_sleep_forever();
	return 0;
}
