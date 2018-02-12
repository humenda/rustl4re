#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

/* Hmm: stdlib.h defines a structure with __init as member... */
#undef __init

#include <stdlib.h>
#include <stddef.h>
#include <linux/init.h>

#define __init

#define GFP_KERNEL  0xabcde

static inline void *kmalloc(size_t size, int flags)
{
	return malloc(size);
}

static inline void kfree(void *p)
{
	free(p);
}

#endif

