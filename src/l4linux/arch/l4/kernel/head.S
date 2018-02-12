/*
 *  This file is questionable as we also could define these memory areas
 *  in C files, on the other hand this "asm" code looks very portable...
 */
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_X86_32
#include <asm/page_types.h>

/* Physical address */
//#define pa(X) ((X) - __PAGE_OFFSET)
#define pa(X) ((X))

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

__REFDATA
	.align 4

/*
 * BSS section
 */
__PAGE_ALIGNED_BSS
	.align PAGE_SIZE
#ifdef CONFIG_X86_PAE
initial_pg_pmd:
	.fill 1024*KPMDS,4,0
#else
ENTRY(initial_page_table)
	.fill 1024,4,0
#endif
initial_pg_fixmap:
	.fill 1024,4,0
ENTRY(empty_zero_page)
	.fill 4096,1,0
ENTRY(swapper_pg_dir)
	.fill 1024,4,0
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
.section .data.head, "wa"
.p2align 12
ENTRY(swapper_pg_dir)
	.fill 4096,4,0
#endif /* ARM */
