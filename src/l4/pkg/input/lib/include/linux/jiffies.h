#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

#include <asm/param.h>

extern unsigned long volatile jiffies;

#define time_after(a,b)   ((long)(b) - (long)(a) < 0)
#define time_before(a,b)  time_after(b,a)

static inline unsigned int msecs_to_jiffies(const unsigned int m)
{
	return (m * HZ) / 1000;
}

#endif
