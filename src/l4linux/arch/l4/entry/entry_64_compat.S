#include <linux/linkage.h>

/* For traps.c */
SYM_CODE_START(entry_SYSENTER_compat)
	nop
SYM_INNER_LABEL(__end_entry_SYSENTER_compat, SYM_L_GLOBAL)
SYM_CODE_END(entry_SYSENTER_compat)
