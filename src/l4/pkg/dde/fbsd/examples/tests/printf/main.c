#include <sys/types.h>
#include <sys/systm.h>

#include <l4/dde/dde.h>
#include <l4/sys/types.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>

l4_ssize_t l4libc_heapsize = 1*1024*1024;

int main(int argc, char **argv) {
	ddekit_init();
	bsd_dde_init();

	printf("printf seven: \"%d\"", 7);
	log(20, "level %d logging", 20);
	uprintf("uprintf seven: \"%d\"", 7);

	return 0;
}
