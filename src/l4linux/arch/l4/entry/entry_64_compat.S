#include <linux/linkage.h>

/* For traps.c */
ENTRY(entry_SYSENTER_compat)
	nop
GLOBAL(__end_entry_SYSENTER_compat)
ENDPROC(entry_SYSENTER_compat)
