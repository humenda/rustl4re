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

#include <linux/sched.h>

DEFINE_RWLOCK(tasklist_lock);

asmlinkage void preempt_schedule(void)
{
	WARN_UNIMPL;
}


/* Our version of scheduler invocation.
 *
 * Scheduling is performed by Fiasco, so we don't care about it as long as
 * a thread is running. If a task becomes TASK_INTERRUPTIBLE or
 * TASK_UNINTERRUPTIBLE, we make sure that the task does not become
 * scheduled by locking the task's sleep lock.
 */
asmlinkage void schedule(void)
{
	dde26_thread_data *t = lxtask_to_ddethread(current);

	switch (current->state) {
		case TASK_RUNNING:
			ddekit_thread_schedule();
			break;
		case TASK_INTERRUPTIBLE:
		case TASK_UNINTERRUPTIBLE:
			ddekit_sem_down(SLEEP_LOCK(t));
			break;
		default:
			panic("current->state = %d --- unknown state\n", current->state);
	}
}


/** yield the current processor to other threads.
 *
 * this is a shortcut for kernel-space yielding - it marks the
 * thread runnable and calls sys_sched_yield().
 */
void __sched yield(void)
{
	set_current_state(TASK_RUNNING);
	ddekit_yield();
}


/***
 * try_to_wake_up - wake up a thread
 * @p: the to-be-woken-up thread
 * @state: the mask of task states that can be woken
 * @sync: do a synchronous wakeup?
 */
int try_to_wake_up(struct task_struct *p, unsigned int state, int sync)
{
	Assert(p);
	dde26_thread_data *t = lxtask_to_ddethread(p);

	Assert(t);
	Assert(SLEEP_LOCK(t));

	p->state = TASK_RUNNING;
	ddekit_sem_up(SLEEP_LOCK(t));

	return 0;
}


static void process_timeout(unsigned long data)
{
	wake_up_process((struct task_struct *)data);
}


signed long __sched schedule_timeout(signed long timeout)
{
	struct timer_list timer;
	unsigned long expire = timeout + jiffies;

	setup_timer(&timer, process_timeout, (unsigned long)current);
	timer.expires = expire;

	switch(timeout)
	{
		/*
		 * Hah!
		 *
		 * Specifying a @timeout value of %MAX_SCHEDULE_TIMEOUT will schedule
		 * the CPU away without a bound on the timeout. In this case the return
		 * value will be %MAX_SCHEDULE_TIMEOUT.
		 */
		case MAX_SCHEDULE_TIMEOUT:
			schedule();
			break;
		default:
			add_timer(&timer);
			schedule();
			del_timer(&timer);
			break;
	}

	timeout = expire - jiffies;

	return timeout < 0 ? 0 : timeout;
}


signed long __sched schedule_timeout_interruptible(signed long timeout)
{
	__set_current_state(TASK_INTERRUPTIBLE);
	return schedule_timeout(timeout);
}


signed long __sched schedule_timeout_uninterruptible(signed long timeout)
{
	__set_current_state(TASK_UNINTERRUPTIBLE);
	return schedule_timeout(timeout);
}

/** Tasks may be forced to run only on a certain no. of CPUs. Since
 *  we only emulate a SMP-environment for the sake of having multiple
 *  threads, we do not need to implement this.
 */
int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask)
{
	return 0;
}

void set_user_nice(struct task_struct *p, long nice)
{
	//WARN_UNIMPL;
}

void __sched io_schedule(void)
{
#ifndef DDE_LINUX
  struct rq *rq = &__raw_get_cpu_var(runqueues);

  delayacct_blkio_start();
  atomic_inc(&rq->nr_iowait);
#endif
  schedule();
#ifndef DDE_LINUX
  atomic_dec(&rq->nr_iowait);
  delayacct_blkio_end();
#endif
}

long __sched io_schedule_timeout(long timeout)
{
	WARN_UNIMPL;
	return -1;
}

extern int sched_setscheduler_nocheck(struct task_struct *t, int flags,
									  struct sched_param *p)
{
	WARN_UNIMPL;
	return -1;
}

void ignore_signals(struct task_struct *t) { }
