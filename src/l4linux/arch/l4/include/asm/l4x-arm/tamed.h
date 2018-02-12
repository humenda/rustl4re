/*
 * Architecture specific handling for tamed mode for ARM.
 */
#ifndef __ASM_L4__L4X_ARM__TAMED_H__
#define __ASM_L4__L4X_ARM__TAMED_H__

#ifndef L4X_TAMED_LABEL
#error Only use from within tamed.c!
#endif

#include <l4/sys/atomic.h>

/* Do not use atomic.h functions here as they use the locking we try to
 * implement first here. */
static inline int l4x_atomic_inc(volatile long int *val)
{
	return l4_atomic_add(val, 1);
}

static inline int l4x_atomic_dec(volatile long int *val)
{
	return l4_atomic_add(val, -1);
}

static inline void l4x_tamed_sem_down(void)
{
	l4_msgtag_t t;
	l4_umword_t l;
	unsigned cpu = current_thread_info()->cpu;
	while (1) {
		if (likely(l4x_atomic_dec(&tamed_per_nr(cli_lock,
		                          get_tamer_nr(cpu)).sem.counter)
		           >= 0))
			break;
#ifdef CONFIG_L4_DEBUG_TAMED_COUNT_INTERRUPT_DISABLE
		cli_taken++;
#endif
		t = l4_msgtag((l4x_prio_current() << 4) | 1, 0, 0, 0);
		t = l4_ipc(tamed_per_nr(cli_sem_thread_id,
		                        get_tamer_nr(cpu)),
		           l4_utcb(),
		           L4_SYSF_CALL, l4x_cap_current(),
		           t, &l, L4_IPC_NEVER);
		if (unlikely(l4_ipc_error(t, l4_utcb())))
			outstring("l4x_tamed_sem_down ipc failed\n");

		if (l4_msgtag_label(t) == 1)
			break;
	}
}


static inline void l4x_tamed_sem_up(void)
{
	l4_umword_t l;
	unsigned cpu = current_thread_info()->cpu;

	if (unlikely(l4x_atomic_inc(&tamed_per_nr(cli_lock,
	                            get_tamer_nr(cpu)).sem.counter)
	             <= 0))
		if (l4_ipc_error(l4_ipc(tamed_per_nr(cli_sem_thread_id,
		                                     get_tamer_nr(cpu)),
		                        l4_utcb(),
		                        L4_SYSF_CALL, l4x_cap_current(),
		                        l4_msgtag((l4x_prio_current() << 4) | 2, 0, 0, 0),
		                        &l, L4_IPC_NEVER),
		                 l4_utcb()))
			outstring("l4x_tamed_sem_up ipc failed\n");
}
#endif /* ! __ASM_L4__L4X_ARM__TAMED_H__ */
