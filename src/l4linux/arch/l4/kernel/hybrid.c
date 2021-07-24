/*
 * Some code for handling hybrid tasks.
 */

/*
 * Hybrid tasks needs to be scanned regularly because they may get signals
 * and need to be interrupted.
 */

#include <linux/list.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <asm/generic/hybrid.h>

#include <l4/sys/thread.h>

struct l4x_hybrid_list {
	struct list_head list;         /* List of tasks */
	struct task_struct *task;      /* task */
};

static struct l4x_hybrid_list l4x_hybrid_list_head = {
	.list    = LIST_HEAD_INIT(l4x_hybrid_list_head.list),
};

static DEFINE_SPINLOCK(list_lock);

static struct l4x_hybrid_list *task_get(struct task_struct *t)
{
	struct l4x_hybrid_list *e;

	list_for_each_entry(e, &l4x_hybrid_list_head.list, list)
		if (e->task == t)
			return e;

	return NULL;
}

void l4x_hybrid_add(struct task_struct *task)
{
	struct l4x_hybrid_list *n;

	n = kmalloc(sizeof(*n), GFP_KERNEL);
	BUG_ON(!n);

	n->task     = task;

	spin_lock(&list_lock);
	list_add_tail(&n->list, &l4x_hybrid_list_head.list);
	spin_unlock(&list_lock);
}

int l4x_hybrid_remove(struct task_struct *t)
{
	struct l4x_hybrid_list *n;

	spin_lock(&list_lock);

	if (!(n = task_get(t))) {
		spin_unlock(&list_lock);
		return 0;
	}

	list_del(&n->list);
	kfree(n);

	spin_unlock(&list_lock);
	return 1;
}

/*
 * seq_file debugging
 */
int l4x_hybrid_seq_show(struct seq_file *m, void *v)
{
	struct l4x_hybrid_list *n;

	spin_lock(&list_lock);

	list_for_each_entry(n, &l4x_hybrid_list_head.list, list) {
		if (n->task)
			seq_printf(m, "%5d (%s)\n",
				   n->task->pid, n->task->comm);
	}

	spin_unlock(&list_lock);
	return 0;
}

/* -------------------------------------------- */

static void l4x_hybrid_wakeup_task(struct task_struct *p)
{
#ifdef CONFIG_L4_VCPU
	l4_thread_ex_regs(p->thread.l4x.hyb_user_thread_id,
	                  ~0UL, ~0UL,
	                  L4_THREAD_EX_REGS_TRIGGER_EXCEPTION
	                  | L4_THREAD_EX_REGS_CANCEL);
#else
	l4_thread_ex_regs(p->thread.l4x.user_thread_id,
	                  ~0UL, ~0UL,
	                  L4_THREAD_EX_REGS_TRIGGER_EXCEPTION
	                  | L4_THREAD_EX_REGS_CANCEL);
#endif
}

/* Only works for the first 32/64 sigs because of the usage of
 * sigtestsetmask but this is ok for our usage with only
 * SIGINT, SIGTERM and SIGKILL */
static const unsigned long sigs_for_interrupt
	= sigmask(SIGINT) | sigmask(SIGTERM) | sigmask(SIGKILL);

static inline void l4x_hybrid_check_task(struct task_struct *p)
{
	if (p && signal_pending(p) && p->thread.l4x.hybrid_sc_in_prog
	    && (sigtestsetmask(&p->pending.signal, sigs_for_interrupt)
		|| sigtestsetmask(&p->signal->shared_pending.signal,
		                  sigs_for_interrupt)))
		l4x_hybrid_wakeup_task(p);
}

/*
 * Scan through all hybrid threads and wake any up that have a signal
 * pending.
 */
void l4x_hybrid_scan_signals(void)
{
	struct l4x_hybrid_list *n;

	spin_lock(&list_lock);
	list_for_each_entry(n, &l4x_hybrid_list_head.list, list)
		l4x_hybrid_check_task(n->task);
	spin_unlock(&list_lock);
}
