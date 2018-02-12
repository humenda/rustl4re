#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <asm/semaphore.h>

#include <l4/sys/kdebug.h> /* for enter_kdebug() */

#ifndef PIT_TICK_RATE
# define PIT_TICK_RATE 1193182
#endif

#define WARN_ON(x)                     \
	do {                           \
	    if (x)                     \
	      enter_kdebug("WARN_ON"); \
	} while (0)

