/**
 * The functions regarding DDE/BSD initialization are found here.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <machine/bus.h>

#include <dde_fbsd/sysinit.h>
#include <dde_fbsd/cold.h>
#include <dde_fbsd/printf.h>

#include <l4/dde/fbsd/bsd_dde.h>

// from kern/subr_param.c:68
// number of ticks per second
int     hz;
// exported in ddekit (io info page)
extern int HZ;

// from i386/i386/machdep.c:184
// memory size in number of pages
// set bus maximum so busdma_machdep works in all cases
// can also be set to the real value
long Maxmem = atop(BUS_SPACE_MAXADDR);

void bsd_dde_init() {
	// maybe we want to print debugging messages so initialize this first
	bsd_init_printf();

	// initialize hz with ddekit (io info page) value
	hz = HZ;

	// let's have curthread work, some KASSERTs check this
	bsd_assign_thread(&thread0);

	// call the bsd SYSINITs
	bsd_call_sysinits();

	// leaving bsd, unlocking Giant
	mtx_unlock(&Giant);
}

