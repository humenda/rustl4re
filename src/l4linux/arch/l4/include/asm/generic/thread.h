#pragma once

#include <linux/threads.h>

#include <l4/sys/types.h>

struct l4x_thread {
#ifndef CONFIG_L4_VCPU
	l4_cap_idx_t		user_thread_id;		/* L4 Thread ID of the p
rocess */
	l4_cap_idx_t		user_thread_ids[NR_CPUS];
	unsigned long		threads_up;
	unsigned int		initial_state_set : 1;	/* 1 if initial state was set */
	unsigned int		started : 1;		/* set if created successfully */
#endif
	unsigned int		is_hybrid : 1;		/* set if hybrid task */
	unsigned int		hybrid_sc_in_prog : 1;	/* l4 syscall in progress  */
#ifdef CONFIG_L4_VCPU
	l4_cap_idx_t		hyb_user_thread_id;
#endif
};
