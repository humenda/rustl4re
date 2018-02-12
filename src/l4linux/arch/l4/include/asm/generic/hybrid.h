/*
 * Header file for hybrid functions.
 */
#ifndef __ASM_L4__GENERIC__HYBRID_H__
#define __ASM_L4__GENERIC__HYBRID_H__

#include <linux/sched.h>
#include <linux/seq_file.h>

#include <l4/sys/types.h>

void l4x_hybrid_add(struct task_struct *task);
int  l4x_hybrid_remove(struct task_struct *task);
int  l4x_hybrid_seq_show(struct seq_file *m, void *v);

void l4x_hybrid_scan_signals(void);

static inline void l4x_hybrid_do_regular_work(void)
{
	l4x_hybrid_scan_signals();
}

#endif /* ! __ASM_L4__GENERIC__HYBRID_H__ */
