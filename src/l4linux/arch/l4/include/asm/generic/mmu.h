#ifndef __INCLUDE__ASM_L4__GENERIC__MMU_H__
#define __INCLUDE__ASM_L4__GENERIC__MMU_H__

#include <l4/sys/types.h>

enum l4x_unmap_mode_enum {
	L4X_UNMAP_MODE_IMMEDIATELY, L4X_UNMAP_MODE_SKIP,
};

void l4x_unmap_log_flush(void);
void l4x_pte_check_empty(struct mm_struct *mm);

#ifdef CONFIG_ARM
#define INIT_MM_CONTEXT(name) \
	.context.task = L4_INVALID_CAP,
#endif /* ARM */

#endif /* ! __INCLUDE__ASM_L4__GENERIC__MMU_H__ */
