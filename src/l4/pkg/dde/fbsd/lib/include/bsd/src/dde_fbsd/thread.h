#ifndef _dde_fbsd_thread_h
#define _dde_fbsd_thread_h

#include <sys/proc.h>

#include <l4/dde/ddekit/thread.h>

struct proc *bsd_alloc_proc(void);
void bsd_free_proc(struct proc *p);
void bsd_init_proc(struct proc *p);
void bsd_deinit_proc(struct proc *p);

struct thread *bsd_alloc_thread(void);
void bsd_free_thread(struct thread *td);
void bsd_init_thread_proc(struct thread *td, struct proc *p);
void bsd_init_thread(struct thread *td);
void bsd_deinit_thread(struct thread *td);

void bsd_assign_thread(struct thread *td);
void bsd_thread_setup_myself(void);
struct thread *bsd_get_curthread(void);
struct thread *bsd_get_thread(ddekit_thread_t *tp);

#endif
