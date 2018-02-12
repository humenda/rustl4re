#ifndef __ASM_L4__GENERIC__STATS_H__
#define __ASM_L4__GENERIC__STATS_H__

#ifdef CONFIG_L4_DEBUG_STATS

#include <l4/util/atomic.h>
#include <linux/types.h>

struct l4x_debug_stats {
	uint32_t l4x_debug_stats_suspend;
	uint32_t l4x_debug_stats_pagefault;
	uint32_t l4x_debug_stats_exceptions;
	uint32_t l4x_debug_stats_pagefault_but_in_PTs;
	uint32_t l4x_debug_stats_pagefault_write;
};

extern struct l4x_debug_stats l4x_debug_stats_data;

#define CONSTRUCT_ONE(name);						\
	extern uint32_t name;						\
	static inline void name##_hit(void)				\
	{								\
		++l4x_debug_stats_data.name;				\
	}								\
	static inline uint32_t name##_get(void)				\
	{								\
		return l4x_debug_stats_data.name;			\
	}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

struct l4x_stats_debugfs_item {
	const char *name;
	int offset;
	int width;
	struct dentry *dentry;
};

extern struct dentry *l4x_debugfs_dir;

#endif

#else

#define CONSTRUCT_ONE(name);						\
	static inline void name##_hit(void)				\
	{								\
	}								\
	static inline uint32_t name##_get(void)				\
	{								\
		return 0;						\
	}

#endif /* CONFIG_L4_DEBUG_STATS */

CONSTRUCT_ONE(l4x_debug_stats_suspend);
CONSTRUCT_ONE(l4x_debug_stats_pagefault);
CONSTRUCT_ONE(l4x_debug_stats_exceptions);
CONSTRUCT_ONE(l4x_debug_stats_pagefault_but_in_PTs);
CONSTRUCT_ONE(l4x_debug_stats_pagefault_write);

#undef CONSTRUCT_ONE

#endif /* ! __ASM_L4__GENERIC__STATS_H__ */
