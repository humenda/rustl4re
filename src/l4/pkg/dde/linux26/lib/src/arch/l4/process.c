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

#include <l4/dde/dde.h>
#include <l4/dde/linux26/dde26.h>

#include <asm/atomic.h>

#include <linux/init_task.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/vmalloc.h>

#include "local.h"

struct ddekit_thread {
        long pthread;
        void *data;
        void *stack;
        void *sleep_cv;
        const char *name;
};
struct ddekit_thread *ddekit_thread_myself(void);

/*****************************************************************************
 ** Current() implementation                                                **
 *****************************************************************************/
struct thread_info *current_thread_info(void)
{
	dde26_thread_data *cur = (dde26_thread_data *)ddekit_thread_get_my_data();
	return &LX_THREAD(cur);
}

struct task_struct *get_current(void)
{
	return current_thread_info()->task;
}

/*****************************************************************************
 ** PID-related stuff                                                       **
 **                                                                         **
 ** Linux manages lists of PIDs that are handed out to processes so that at **
 ** a later point it is able to determine which task_struct belongs to a    **
 ** certain PID. We implement this with a single list holding the mappings  **
 ** for all our threads.                                                    **
 *****************************************************************************/

LIST_HEAD(_pid_task_list);
ddekit_lock_t _pid_task_list_lock;

/** PID to task_struct mapping */
struct pid2task
{
    struct list_head list;    /**< list data */
    struct pid *pid;          /**< PID */
    struct task_struct *ts;   /**< task struct */
};

struct pid init_struct_pid = INIT_STRUCT_PID;

void put_pid(struct pid *pid)
{
	if (pid)
		atomic_dec(&pid->count);
	// no freeing here, our struct pid's are always allocated as
	// part of the dde26_thread_data
}

/** Attach PID to a certain task struct. */
void attach_pid(struct task_struct *task, enum pid_type type
                __attribute__((unused)), struct pid *pid)
{
	/* Initialize a new pid2task mapping */
	struct pid2task *pt = kmalloc(sizeof(struct pid2task), GFP_KERNEL);
	pt->pid = get_pid(pid);
	pt->ts = task;

	/* add to list */
	ddekit_lock_lock(&_pid_task_list_lock);
	list_add(&pt->list, &_pid_task_list);
	ddekit_lock_unlock(&_pid_task_list_lock);
}

/** Detach PID from a task struct. */
void detach_pid(struct task_struct *task, enum pid_type type __attribute__((unused)))
{
	struct list_head *p, *n, *h;

	h = &_pid_task_list;
	
	ddekit_lock_lock(&_pid_task_list_lock);
	/* search for mapping with given task struct and free it if necessary */
	list_for_each_safe(p, n, h) {
		struct pid2task *pt = list_entry(p, struct pid2task, list);
		if (pt->ts == task) {
			put_pid(pt->pid);
			list_del(p);
			kfree(pt);
			break;
		}
	}
	ddekit_lock_unlock(&_pid_task_list_lock);
}

struct task_struct *find_task_by_pid_type(int type, int nr)
{
	struct list_head *h, *p;
	h = &_pid_task_list;

	ddekit_lock_lock(&_pid_task_list_lock);
	list_for_each(p, h) {
		struct pid2task *pt = list_entry(p, struct pid2task, list);
		if (pid_nr(pt->pid) == nr) {
			ddekit_lock_unlock(&_pid_task_list_lock);
			return pt->ts;
		}
	}
	ddekit_lock_unlock(&_pid_task_list_lock);

	return NULL;
}


struct task_struct *find_task_by_pid_ns(int nr, struct pid_namespace *ns)
{
    /* we don't implement PID name spaces */
    return find_task_by_pid_type(0, nr);
}

struct task_struct *find_task_by_pid(int nr)
{
	return find_task_by_pid_type(0, nr);
}

/*****************************************************************************
 ** kernel_thread() implementation                                          **
 *****************************************************************************/
/* Struct containing thread data for a newly created kthread. */
struct __kthread_data
{
	int (*fn)(void *);
	void              *arg;
	ddekit_lock_t      lock;
	dde26_thread_data *kthread;	
};

/** Counter for running kthreads. It is used to create unique names
 *  for kthreads.
 */
static atomic_t kthread_count = ATOMIC_INIT(0);

/** Entry point for new kernel threads. Make this thread a DDE26
 *  worker and then execute the real thread fn.
 */
static void __kthread_helper(void *arg)
{
	struct __kthread_data *k = (struct __kthread_data *)arg;

	/*
	 * Make a copy of the fn and arg pointers, as the kthread struct is
	 * deleted by our parent after notifying it and this may happen before we
	 * get to execute the function.
	 */
	int (*_fn)(void*)   = k->fn;
	void *_arg          = k->arg;

	l4dde26_process_add_worker();

	/*
	 * Handshake with creator - we store our thread data in the
	 * kthread struct and then unlock the lock to notify our
	 * creator about completing setup
	 */
	k->kthread = (dde26_thread_data *)ddekit_thread_get_my_data();
	ddekit_lock_unlock(&k->lock);
	
	do_exit(_fn(_arg));
}

/** Our implementation of Linux' kernel_thread() function. Setup a new
 * thread running our __kthread_helper() function.
 */
int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	ddekit_thread_t *t;
	char name[20];
	struct __kthread_data *kt = vmalloc(sizeof(struct __kthread_data));
	ddekit_lock_t lock;

	/* Initialize (and grab) handshake lock */
	ddekit_lock_init(&lock);
	ddekit_lock_lock(&lock);

    int threadnum = atomic_inc_return(&kthread_count);
	kt->fn        = fn;
	kt->arg       = arg;
	kt->lock      = lock; // Copy lock ptr, note that kt is freed by the
	                      // new thread, so we MUST NOT use kt->lock after
	                      // this point!

	snprintf(name, 20, ".kthread%x", threadnum);
	t = ddekit_thread_create(__kthread_helper,
	                         (void *)kt, name, 0);
	Assert(t);

	ddekit_lock_lock(&lock);
	ddekit_lock_deinit(&lock);

	return pid_nr(VPID_P(kt->kthread));
}

/** Our implementation of exit(). For DDE purposes this only relates
 * to kernel threads.
 */
void do_exit(long code)
{
	ddekit_thread_t *t = DDEKIT_THREAD(lxtask_to_ddethread(current));
//	printk("Thread %s exits with code %x\n", ddekit_thread_get_name(t), code);

	/* do some cleanup */
	detach_pid(current, 0);
	
	/* goodbye, cruel world... */
	ddekit_thread_exit();
}

/*****************************************************************************
 ** Misc functions                                                          **
 *****************************************************************************/

#ifndef ARCH_arm
void dump_stack(void)
{
}
#else
void __backtrace(void)
{
}
#endif


char *get_task_comm(char *buf, struct task_struct *tsk)
{
	char *ret;
    /* buf must be at least sizeof(tsk->comm) in size */
    task_lock(tsk);
    ret = strncpy(buf, tsk->comm, sizeof(tsk->comm));
    task_unlock(tsk);
	return ret;
}


void set_task_comm(struct task_struct *tsk, char *buf)
{
    task_lock(tsk);
    strlcpy(tsk->comm, buf, sizeof(tsk->comm));
    task_unlock(tsk);
}


/*****************************************************************************
 ** DDEKit gluecode, init functions                                         **
 *****************************************************************************/
/* Initialize a dde26 thread. 
 *
 * - Allocate thread data, as well as a Linux task struct, 
 * - Fill in default values for thread_info, and task,
 * - Adapt task struct's thread_info backreference
 * - Initialize the DDE sleep lock
 */
static dde26_thread_data *init_dde26_thread(void)
{
	/*
	 * Virtual PID counter
	 */
	static atomic_t pid_counter = ATOMIC_INIT(0);
	dde26_thread_data *t = vmalloc(sizeof(dde26_thread_data));
	Assert(t);
	
	memcpy(&t->_vpid, &init_struct_pid, sizeof(struct pid));
	t->_vpid.numbers[0].nr = atomic_inc_return(&pid_counter);
	
	memcpy(&LX_THREAD(t), &init_thread, sizeof(struct thread_info));

	LX_TASK(t) = vmalloc(sizeof(struct task_struct));
	Assert(LX_TASK(t));

	memcpy(LX_TASK(t), &init_task, sizeof(struct task_struct));

	/* nice: Linux backreferences a task`s thread_info from the
	*        task struct (which in turn can be found using the
	*        thread_info...) */
	LX_TASK(t)->stack = &LX_THREAD(t);

	/* initialize this thread's sleep lock */
	SLEEP_LOCK(t) = ddekit_sem_init(0);

	return t;
}

/* Process setup for worker threads */
int l4dde26_process_add_worker(void)
{
	dde26_thread_data *cur = init_dde26_thread();

	/* If this function is called for a kernel_thread, the thread already has
	 * been set up and we just need to store a reference to the ddekit struct.
	 * However, this function may also be called directly to turn an L4 thread
	 * into a DDE thread. Then, we need to initialize here. */
	cur->_ddekit_thread = ddekit_thread_myself();
	if (cur->_ddekit_thread == NULL)
		cur->_ddekit_thread = ddekit_thread_setup_myself(".dde26_thread");
	Assert(cur->_ddekit_thread);

	ddekit_thread_set_my_data(cur);

	attach_pid(LX_TASK(cur), 0, &cur->_vpid);

	/* Linux' default is to have this set to 1 initially and let the
	 * scheduler set this to 0 later on.
	 */
	current_thread_info()->preempt_count = 0;

	return 0;
}


/**
 * Add an already existing DDEKit thread to the set of threads known to the
 * Linux environment. This is used for the timer thread, which is actually a
 * DDEKit thread, but Linux code shall see it as a Linux thread as well.
 */
int l4dde26_process_from_ddekit(ddekit_thread_t *t)
{
	Assert(t);

	dde26_thread_data *cur = init_dde26_thread();
	cur->_ddekit_thread = t;
	ddekit_thread_set_data(t, cur);
	attach_pid(LX_TASK(cur), 0, &cur->_vpid);

	return 0;
}

/** Function to initialize the first DDE process.
 */
void __init l4dde26_process_init(void)
{
	ddekit_lock_init_unlocked(&_pid_task_list_lock);

	int kthreadd_pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
	kthreadd_task = find_task_by_pid(kthreadd_pid);

	l4dde26_process_add_worker();
}

DEFINE_PER_CPU(int, cpu_number);

/* dde_process_initcall(l4dde26_process_init); */
