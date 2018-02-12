#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

struct __wait_queue_entry_t {
	l4_cap_idx_t tid;
	struct __wait_queue_entry_t *next;
};

struct __wait_queue_head_t {
	struct __wait_queue_entry_t *first;
	struct __wait_queue_head_t  *next;
};

typedef struct __wait_queue_entry_t wait_queue_entry_t;
typedef struct __wait_queue_head_t wait_queue_head_t;

extern l4_cap_idx_t wait_thread;

void wake_up(wait_queue_head_t *wq);

#define init_waitqueue_head(wq)						\
	do {								\
		(wq)->first = 0;					\
		(wq)->next  = 0;					\
	} while (0)

#include <l4/sys/ipc.h>
#include <pthread.h>
#include <pthread-l4.h>

#define wait_event_timeout(wq, condition, timeout)			\
({									\
	unsigned stop, end = jiffies + timeout;				\
	l4_umword_t dummy;						\
	l4_msgtag_t r;						        \
	wait_queue_entry_t wqe;						\
	while (!(condition) && (end - jiffies < 1000000000))		\
	{								\
		l4_utcb_mr()->mr[0] = (l4_umword_t)&(wq);               \
		l4_utcb_mr()->mr[1] = (l4_umword_t)&(wqe);	        \
		r = l4_ipc(wait_thread, l4_utcb(), L4_SYSF_CALL,        \
			   pthread_l4_cap(pthread_self()),            \
                           l4_msgtag(0, 2, 0, 0),                       \
                           &dummy, L4_IPC_NEVER);                       \
		if (l4_ipc_error(r, l4_utcb()))				\
			enter_kdebug("wait_timeout");			\
 	}								\
	stop = jiffies;							\
	if (stop < end)							\
		end -= stop;						\
	else								\
		end = 0;						\
	end;								\
})

#endif
