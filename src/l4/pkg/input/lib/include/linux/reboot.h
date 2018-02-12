#ifndef _LINUX_REBOOT_H
#define _LINUX_REBOOT_H

#include <linux/notifier.h>

static inline int register_reboot_notifier(struct notifier_block *n)
{
	return 0;
}
static inline int unregister_reboot_notifier(struct notifier_block *n)
{
	return 0;
}

#endif

