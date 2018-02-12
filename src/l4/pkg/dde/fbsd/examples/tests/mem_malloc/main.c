#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/stdarg.h>

#include <l4/dde/dde.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>
#include <l4/sys/l4int.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

MALLOC_DECLARE(M_TST);
MALLOC_DEFINE(M_TST, "short", "long");

int main(int argc, char **argv) {
	void *p;
	int i;

	LOG_Enter("");

	ddekit_init();
	bsd_dde_init();

	for (i=0; i<=5; i++) {
		p = malloc(20, M_TST, M_WAITOK);
		LOGl("%p", p);
		free(p, M_TST);
		p = malloc(20, M_TST, M_WAITOK);
		LOGl("%p", p);
	}

	return 0;
}
