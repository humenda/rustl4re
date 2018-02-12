#ifndef _ASMARM_CURRENT_H
#define _ASMARM_CURRENT_H

#include <linux/thread_info.h>

#ifndef DDE_LINUX
static inline struct task_struct *get_current(void) __attribute_const__;

static inline struct task_struct *get_current(void)
{
	return current_thread_info()->task;
}
#else
struct task_struct *get_current(void);
#endif


#define current (get_current())

#endif /* _ASMARM_CURRENT_H */
