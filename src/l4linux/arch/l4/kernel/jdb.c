#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <asm/generic/jdb.h>

#include <asm/generic/log.h>
#include <l4/log/log.h>

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "l4x."


static char jdb_prefix[9] = "l4lx";
module_param_string(jdb_prefix, jdb_prefix, sizeof(jdb_prefix), 0);

static int jdb_prefix_len;

enum { JDB_NAME_LEN = 20 };

struct cap_and_name {
	l4_cap_idx_t cap;
	char name[JDB_NAME_LEN];
};

static struct cap_and_name st[4] __initdata;
static unsigned st_idx __initdata;
static bool inited;

__ref void
l4x_dbg_set_object_name_delim(l4_cap_idx_t cap, char delim,
                              const char *fmt, ...)
{
	char n[JDB_NAME_LEN];
	va_list ap;
	int off = jdb_prefix_len + !!delim;

	if (jdb_prefix_len)
		strncpy(n, jdb_prefix, jdb_prefix_len);
	if (delim)
		n[jdb_prefix_len] = delim;

	va_start(ap, fmt);
	vsnprintf(n + off, sizeof(n) - off - 1, fmt, ap);
	va_end(ap);
	n[sizeof(n) - 1] = '\0';

	if (inited || st_idx >= ARRAY_SIZE(st))
		l4_debugger_set_object_name(cap, n);
	else {
		st[st_idx].cap = cap;
		strncpy(st[st_idx].name, n, sizeof(st[0].name));
		st[st_idx++].name[sizeof(st[0].name) - 1] = 0;
	}
}

static int __init init_jdb_names(void)
{
	unsigned i;

	jdb_prefix_len = min((int)strlen(jdb_prefix), JDB_NAME_LEN - 2);
	inited = true;

	for (i = 0; i < st_idx; ++i)
		L4XV_FN_v(l4x_dbg_set_object_name_delim(st[i].cap, 0,
		                                        "%s", st[i].name));

	L4XV_FN_v(l4x_dbg_set_object_name(l4re_env()->main_thread, "main"));
	L4XV_FN_v(l4x_dbg_set_object_name(L4_BASE_TASK_CAP, "kern"));

	return 0;
}

early_initcall(init_jdb_names);
