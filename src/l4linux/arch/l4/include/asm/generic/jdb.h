#ifndef __ASM_L4__GENERIC__JDB_H__
#define __ASM_L4__GENERIC__JDB_H__

#include <linux/compiler.h>
#include <l4/sys/types.h>

extern l4_cap_idx_t l4x_jdb_cap;

#ifdef CONFIG_L4_DEBUG_REGISTER_NAMES
__printf(3, 4)
void
l4x_dbg_set_object_name_delim(l4_cap_idx_t cap, char delim,
                              const char *fmt, ...);

#else

__printf(3, 4)
static inline
void
l4x_dbg_set_object_name_delim(l4_cap_idx_t cap, char delim,
                              const char *fmt, ...)
{}
#endif

#define l4x_dbg_set_object_name(cap, fmt, ...) \
	l4x_dbg_set_object_name_delim(cap, '.', fmt, ##__VA_ARGS__)

#define l4x_dbg_set_object_name_user(cap, fmt, ...) \
	l4x_dbg_set_object_name_delim(cap, ':', fmt, ##__VA_ARGS__)

#endif /* ! __ASM_L4__GENERIC__JDB_H__ */
