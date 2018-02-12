#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

#include <linux/errno.h>

struct notifier_block
{
	int (*notifier_call)(struct notifier_block *self, unsigned long, void *);
	struct notifier_block *next;
	int priority;
};

#define NOTIFY_DONE		0x0000		/* Don't care */
#define NOTIFY_OK		0x0001		/* Suits me */
#define NOTIFY_STOP_MASK	0x8000		/* Don't call further */
#define NOTIFY_BAD		(NOTIFY_STOP_MASK|0x0002)	/* Bad/Veto action	*/

#define SYS_DOWN	0x0001	/* Notify of system down */
#define SYS_RESTART	SYS_DOWN
#define SYS_HALT	0x0002	/* Notify of system halt */
#define SYS_POWER_OFF	0x0003	/* Notify of system power off */

#endif

