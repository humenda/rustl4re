#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/config.h>
#include <linux/list.h>
//#include <linux/spinlock.h>

struct timer_list {
//	struct list_head entry;
//	unsigned long expires;

//	spinlock_t lock;
//	unsigned long magic;

	void (*function)(unsigned long);
	unsigned long data;

//	struct tvec_t_base_s *base;
};

static inline void init_timer(struct timer_list * timer)
{
#if 0
	timer->base = NULL;
	timer->magic = TIMER_MAGIC;
	spin_lock_init(&timer->lock);
#endif
}

#if 0
static inline int timer_pending(const struct timer_list * timer)
{
	return timer->base != NULL;
}
#endif

//#define add_timer_on(x, y)
static inline int del_timer(struct timer_list *timer)
{
	return 0;
}
static inline int mod_timer(struct timer_list *timer, unsigned long expires)
{
	return 0;
}
#define add_timer(x)
#define del_timer_sync(x) del_timer(x)

#endif

