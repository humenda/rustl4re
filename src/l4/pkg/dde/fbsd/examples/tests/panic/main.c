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

	panic("seven: \"%d\"", 7);

	return 0;
}
