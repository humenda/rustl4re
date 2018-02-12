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

#ifndef __DDE26_LOCAL_H
#define __DDE26_LOCAL_H

#include <linux/sched.h>

#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/block.h>
#include <l4/dde/ddekit/condvar.h>
#include <l4/dde/ddekit/debug.h>
#include <l4/dde/ddekit/initcall.h>
#include <l4/dde/ddekit/interrupt.h>
#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/pci.h>
#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/resources.h>
#include <l4/dde/ddekit/semaphore.h>
#include <l4/dde/ddekit/thread.h>
#include <l4/dde/ddekit/types.h>
#include <l4/dde/ddekit/timer.h>

#include <l4/dde/linux26/dde26.h>

#define DDE_DEBUG     1
#define DDE_FERRET    0

/* Ferret Debugging stuff, note that this is the only point we are using
 * L4 headers directly and only for debugging. */
#if DDE_FERRET
#include <l4/ferret/maj_min.h>
#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>
extern ferret_list_local_t *ferret_ore_sensor;
#endif

/***
 * Internal representation of a Linux kernel thread. This struct
 * contains Linux' data as well as some additional data used by DDE.
 */
typedef struct dde26_thread_data
{
	/* NOTE: _threadinfo needs to be first in this struct! */
	struct thread_info  _thread_info;   ///< Linux thread info (see current())
	ddekit_thread_t    *_ddekit_thread; ///< underlying DDEKit thread
	ddekit_sem_t       *_sleep_lock;    ///< lock used for sleep_interruptible()
	struct pid          _vpid;          ///< virtual PID
} dde26_thread_data;

#define LX_THREAD(thread_data)     ((thread_data)->_thread_info)
#define LX_TASK(thread_data)       ((thread_data)->_thread_info.task)
#define DDEKIT_THREAD(thread_data) ((thread_data)->_ddekit_thread)
#define SLEEP_LOCK(thread_data)    ((thread_data)->_sleep_lock)
#define VPID_P(thread_data)        (&(thread_data)->_vpid)

#if DDE_DEBUG
#define WARN_UNIMPL         printk("unimplemented: %s\n", __FUNCTION__)
#define DEBUG_MSG(msg, ...) printk("%s: \033[36m"msg"\033[0m\n", __FUNCTION__, ##__VA_ARGS__)

#define DECLARE_INITVAR(name) \
	static struct { \
		int _initialized; \
		char *name; \
	} init_##name = {0, #name,}

#define INITIALIZE_INITVAR(name) init_##name._initialized = 1

#define CHECK_INITVAR(name) \
	if (init_##name._initialized == 0) { \
		printk("DDE26: \033[31;1mUsing uninitialized subsystem: "#name"\033[0m\n"); \
		BUG(); \
	}

#else /* !DDE_DEBUG */

#define WARN_UNIMPL                do {} while(0)
#define DEBUG_MSG(...)             do {} while(0)
#define DECLARE_INITVAR(name)
#define CHECK_INITVAR(name)        do {} while(0)
#define INITIALIZE_INITVAR(name)   do {} while(0)

#endif

/* since _thread_info always comes first in the thread_data struct,
 * we can derive the dde26_thread_data from a task struct by simply
 * dereferencing its thread_info pointer
 */
static dde26_thread_data *lxtask_to_ddethread(struct task_struct *t)
{
	return (dde26_thread_data *)(task_thread_info(t));
}

extern struct thread_info init_thread;
extern struct task_struct init_task;

#endif
