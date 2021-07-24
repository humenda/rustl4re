/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/linkage.h>
#include <asm/export.h>

.text

/*
 * Inputs:
 * %esi : memory location to compare
 * %eax : low 32 bits of old value
 * %edx : high 32 bits of old value
 * %ebx : low 32 bits of new value
 * %ecx : high 32 bits of new value
 */
SYM_FUNC_START(cmpxchg8b_emu)

#
# Emulate 'cmpxchg8b (%esi)' on UP except we don't
# set the whole ZF thing (caller will just compare
# eax:edx with the expected value)
#
#ifdef CONFIG_L4
	push %eax
	push %edx
	push %ecx
	call l4x_global_save_flags
	mov  %eax, %edi
	call l4x_global_cli
	pop  %ecx
	pop  %edx
	pop  %eax
#else
	cli
#endif

	cmpl  (%esi), %eax
	jne .Lnot_same
	cmpl 4(%esi), %edx
	jne .Lhalf_same

	movl %ebx,  (%esi)
	movl %ecx, 4(%esi)

#ifdef CONFIG_L4
	push %eax
	push %edx
	mov  %edi, %eax
	call l4x_global_restore_flags
	pop  %edx
	pop  %eax
#else
	popfl
#endif
	ret

.Lnot_same:
	movl  (%esi), %eax
.Lhalf_same:
	movl 4(%esi), %edx

#ifdef CONFIG_L4
	push %eax
	push %edx

	mov  %edi, %eax
	call l4x_global_restore_flags
	pop  %edx
	pop  %eax
#else
	popfl
#endif
	ret

SYM_FUNC_END(cmpxchg8b_emu)
EXPORT_SYMBOL(cmpxchg8b_emu)
