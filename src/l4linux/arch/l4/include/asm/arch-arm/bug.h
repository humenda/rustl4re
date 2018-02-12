/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_L4__ARCH_ARM__BUG_H__
#define __ASM_L4__ARCH_ARM__BUG_H__

#include <asm-arm/bug.h>

/* Fiasco always returns LPAE codes */
#undef FAULT_CODE_ALIGNMENT
#define FAULT_CODE_ALIGNMENT	33
#undef FAULT_CODE_DEBUG
#define FAULT_CODE_DEBUG	34

#endif /* __ASM_L4__ARCH_ARM__BUG_H__ */
