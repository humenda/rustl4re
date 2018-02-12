/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "local.h"

#include <linux/interrupt.h>

/* There are at most 32 softirqs in Linux, but only 6 are really used. */
#define NUM_SOFTIRQS			6

DECLARE_INITVAR(dde26_softirq);

/* softirq threads and their wakeup semaphores */
ddekit_thread_t *dde_softirq_thread;
ddekit_sem_t	*dde_softirq_sem;

/* struct tasklet_head is not defined in a header in Linux 2.6 */
struct tasklet_head
{
	struct tasklet_struct   *list;
	ddekit_lock_t            lock;  /* list lock */
};

/* What to do if a softirq occurs. */
static struct softirq_action softirq_vec[32];

/* tasklet queues for each softirq thread */
struct tasklet_head tasklet_vec;
struct tasklet_head tasklet_hi_vec;

void open_softirq(int nr, void (*action)(struct softirq_action*))
{
	softirq_vec[nr].action = action;
}

static void raise_softirq_irqoff_cpu(unsigned int nr, unsigned int cpu)
{
	CHECK_INITVAR(dde26_softirq);

	/* mark softirq scheduled */
	__raise_softirq_irqoff(nr);
	/* wake softirq thread */
	ddekit_sem_up(dde_softirq_sem);
}

void raise_softirq_irqoff(unsigned int nr)
{
	raise_softirq_irqoff_cpu(nr, 0);
}

void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

/**
 * Initialize tasklet.
 */
void tasklet_init(struct tasklet_struct *t,
                  void (*func)(unsigned long), unsigned long data)
{
	t->next  = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func  = func;
	t->data  = data;
}

void tasklet_kill(struct tasklet_struct *t)
{
	if (in_interrupt())
		printk("Attempt to kill tasklet from interrupt\n");

	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do
			yield();
		while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}

//EXPORT_SYMBOL(tasklet_kill);

/* enqueue tasklet */
static void __tasklet_enqueue(struct tasklet_struct *t, 
                              struct tasklet_head *listhead)
{
	ddekit_lock_lock(&listhead->lock);
	t->next = listhead->list;
	listhead->list = t;
	ddekit_lock_unlock(&listhead->lock);
}

void __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	CHECK_INITVAR(dde26_softirq);

	local_irq_save(flags);

	__tasklet_enqueue(t, &tasklet_vec);
	/* raise softirq */
	raise_softirq_irqoff_cpu(TASKLET_SOFTIRQ, 0);

	local_irq_restore(flags);
}

void __tasklet_hi_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	CHECK_INITVAR(dde26_softirq);

	local_irq_save(flags);
	__tasklet_enqueue(t, &tasklet_hi_vec);
	raise_softirq_irqoff_cpu(HI_SOFTIRQ, 0);
	local_irq_restore(flags);
}

/* Execute tasklets */
static void tasklet_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	ddekit_lock_lock(&tasklet_vec.lock);
	list = tasklet_vec.list;
	tasklet_vec.list = NULL;
	ddekit_lock_unlock(&tasklet_vec.lock);

	while (list) {	
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		ddekit_lock_lock(&tasklet_vec.lock);
		t->next = tasklet_vec.list;
		tasklet_vec.list = t;
		raise_softirq_irqoff_cpu(TASKLET_SOFTIRQ, 0);
		ddekit_lock_unlock(&tasklet_vec.lock);
	}
}


static void tasklet_hi_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	ddekit_lock_lock(&tasklet_hi_vec.lock);
	list = tasklet_hi_vec.list;
	tasklet_hi_vec.list = NULL;
	ddekit_lock_unlock(&tasklet_hi_vec.lock);

	while (list) {	
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		ddekit_lock_lock(&tasklet_hi_vec.lock);
		t->next = tasklet_hi_vec.list;
		tasklet_hi_vec.list = t;
		raise_softirq_irqoff_cpu(HI_SOFTIRQ, 0);
		ddekit_lock_unlock(&tasklet_hi_vec.lock);
	}
}


#define MAX_SOFTIRQ_RETRIES	10

/** Run softirq handlers 
 */
asmlinkage void __do_softirq(void)
{
	int retries = MAX_SOFTIRQ_RETRIES;

	do {
		struct softirq_action *h = softirq_vec;
		unsigned long pending = local_softirq_pending();

		/* reset softirq count */
		set_softirq_pending(0);

		local_irq_enable();

		/* While we have a softirq pending... */
		while (pending) {
			/* need to execute current softirq? */
			if (pending & 1)
				h->action(h);
			/* try next softirq */
			h++;
			/* remove pending flag for last softirq */
			pending >>= 1;
		}

		local_irq_disable();

	/* Somebody might have scheduled another softirq in between
	 * (e.g., an IRQ thread or another tasklet). */
	} while (local_softirq_pending() && --retries);

	/* retries are up. check if interrupts are still pending
	 * and reschedule
	 */
	if(local_softirq_pending()) {
		ddekit_sem_up(dde_softirq_sem);
	}
}


asmlinkage void do_softirq(void)
{
	unsigned long flags;

	local_irq_save(flags);
	if (local_softirq_pending())
		__do_softirq();
	local_irq_restore(flags);
}

/** Softirq thread function.
 *
 * Once started, a softirq thread waits for tasklets to be scheduled
 * and executes them.
 *
 * \param arg	# of this softirq thread so that it grabs the correct lock
 *              if multiple softirq threads are running.
 */
void l4dde26_softirq_thread(void *arg)
{
	printk("Softirq daemon starting\n");
	l4dde26_process_add_worker();

	/* This thread will always be in a softirq, so set the 
	 * corresponding flag right now.
	 */
	preempt_count() |= SOFTIRQ_MASK;

	while(1) {
		ddekit_sem_down(dde_softirq_sem);
		do_softirq();
	}
}

/** Initialize softirq subsystem.
 *
 * Start NUM_SOFTIRQ_THREADS threads executing the \ref l4dde26_softirq_thread
 * function.
 */
void l4dde26_softirq_init(void)
{
	char name[20];

	dde_softirq_sem = ddekit_sem_init(0);

	set_softirq_pending(0);

	ddekit_lock_init_unlocked(&tasklet_vec.lock);
	ddekit_lock_init_unlocked(&tasklet_hi_vec.lock);

	snprintf(name, 20, ".softirqd");
	dde_softirq_thread = ddekit_thread_create(
	                           l4dde26_softirq_thread,
	                           NULL, name, 0);

	open_softirq(TASKLET_SOFTIRQ, tasklet_action);
	open_softirq(HI_SOFTIRQ, tasklet_hi_action);

	INITIALIZE_INITVAR(dde26_softirq);
}
