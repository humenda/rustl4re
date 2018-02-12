/**
 * Various simple dummy implementations. 
 * Some are copied from the respective FreeBSD files.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <machine/md_var.h>
#include <machine/cpu.h>
#include <dev/pci/pcivar.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/kdb.h>
#include <vm/vm.h>
#include <vm/pmap.h>

// don't know where these are defined in FreeBSD
char static_hints[] = "";
int hintmode = 1;            /* 0 = off. 1 = config, 2 = fallback */

// kern/init_main.c:098
int     bootverbose = 1;

// kern/kern_shutdown.c:126
int dumping = 0;                            /* system is dumping */

// from sys/net/netisr.c:56 ff:
/* 
 * debug_mpsafenet controls network subsystem-wide use of the Giant lock,
 * from system calls down to interrupt handlers.  It can be changed only via
 * a tunable at boot, not at run-time, due to the complexity of unwinding.
 * The compiled default is set via a kernel option; right now, the default
 * unless otherwise specified is to run the network stack without Giant.
 */
#ifdef NET_WITH_GIANT
int     debug_mpsafenet = 0;
#else
int     debug_mpsafenet = 1;
#endif

// from i386/i386/vm_machdep.c:745
/*
 * Software interrupt handler for queued VM system processing.
 */   
void  
swi_vm(void *dummy) 
{     
        if (busdma_swi_pending != 0)
                busdma_swi();
}

// kern/kern_prot.c:1246
int suser(struct thread *td) {
	return 0;
}

// pc98/pc98/clock.c:398
void DELAY(int usec) {
	ddekit_thread_usleep(usec);
}


// from dev/pci/pci_user.c
#if __FreeBSD_version < 500000
#define	PCI_CDEV	78
#else
#define	PCI_CDEV	MAJOR_AUTO
#endif

struct cdevsw pcicdev = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_name =	"pci",
};

// from vm/vm_meter.c:067
struct vmmeter cnt = {
	.v_page_count = 1,
};

// originally in assembler code
u_long   intrcnt[1];
char     intrnames[1];
u_long   *eintrcnt = intrcnt;
char     *eintrnames = intrnames;

// from kern/subr_kdb.c:055
int kdb_active = 0;
// from kern/subr_kdb.c:222
void kdb_backtrace() {
}
// from kern/subr_kdb.c:318
void kdb_reenter(void) {
}

// from kern/kern_thread.c:379
void thread_stash(struct thread *td) {
}

// from i386/i386/pmap.c:852
// not needed (only by bus_dmamap_load_uio())
vm_paddr_t pmap_extract(pmap_t pmap, vm_offset_t va) {
	return 0;
}

// from kern/kern_thread.c:1083
int thread_sleep_check(struct thread *td) {
	return 0;
}
