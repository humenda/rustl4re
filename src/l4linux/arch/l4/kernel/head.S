/*
 *  This file is questionable as we also could define these memory areas
 *  in C files, on the other hand this "asm" code looks very portable...
 */
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_X86_32
.section ".text"
	mov 0(%esi), %edi
	mov %edi, __l4_external_resolver

	mov $l4_utcb_wrap, %edi
	mov %edi, 4(%esi)

	mov 8(%esi), %edi
	mov %edi, l4re_global_env

	mov 12(%esi), %edi
	mov %edi, l4lx_kinfo

	jmp main
.previous
#endif /* X86_32 */

#ifdef CONFIG_ARM
.section ".text"
.globl _start
_start:
	ldr r4, [r2, #0]
	ldr r5, __address_res
	str r4, [r5]

	ldr r4, __address_l4_utcb_wrap
	str r4, [r2, #4]

	ldr r4, [r2, #8]
	ldr r5, __address_l4re_global_env
	str r4, [r5]

	ldr r4, [r2, #12]
	ldr r5, __address_l4lx_kinfo
	str r4, [r5]

	b main
__address_res:
	.long __l4_external_resolver
__address_l4_utcb_wrap:
	.long l4_utcb_wrap
__address_l4re_global_env:
	.long l4re_global_env
__address_l4lx_kinfo:
	.long l4lx_kinfo
.previous
.section .data, "wa"
.p2align 12
ENTRY(swapper_pg_dir)
	.fill 4096,4,0
#endif /* ARM */

#ifdef CONFIG_ARM64
.section ".text"
.globl _start
_start:
	ldr x4, [x2, #0]
	ldr x5, __address_res
	str x4, [x5]

	ldr x4, __address_l4_utcb_wrap
	str x4, [x2, #8]

	ldr x4, [x2, #16]
	ldr x5, __address_l4re_global_env
	str x4, [x5]

	ldr x4, [x2, #24]
	ldr x5, __address_l4lx_kinfo
	str x4, [x5]

	b main

__address_res:
	.quad __l4_external_resolver
__address_l4_utcb_wrap:
	.quad l4_utcb_wrap
__address_l4re_global_env:
	.quad l4re_global_env
__address_l4lx_kinfo:
	.quad l4lx_kinfo
.previous
#endif /* ARM64 */
