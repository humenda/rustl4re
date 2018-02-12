#ifndef _dde_fbsd_sysinit_h
#define _dde_fbsd_sysinit_h

#include <sys/kernel.h>

void bsd_register_sysinit(struct sysinit *new);
void bsd_call_sysinits(void);

#define SI_DDE_INTR    0x00000100 // has no dependencies
#define SI_DDE_SCHED   0x00000100 // has no dependencies
#define SI_DDE_UMA     0x00000100 // has no dependencies
#define SI_DDE_THREADS 0x00000100 // has no dependencies
#define SI_DDE_COLD    0x00000100 // has no dependencies
// initialize dde_locks backing statically initialized bsd mutexes
#define SI_DDE_STATLOCK	0x00000100 // has no dependencies
// call mutex_init()
#define SI_DDE_MUTEX   0x00000200 // depends on SI_DDE_STATLOCK (all_mtx)
#define SI_DDE_TIMEOUT SI_SUB_INTRINSIC+1 // depends on SI_SUB_INTRINSIC (proc0)
#define SI_DDE_LAST    0xFFFFFFFF // all initialization finished

#endif
