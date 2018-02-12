/*
 * Copyright (C) 2017 Kernkonzept GmbH.
 * Author: Jakub Jermar <jakub.jermar@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#define __ASSEMBLY__

#include <l4/util/mb_info.h>

	.section .multiboot2, "ax"

#define MB2_HEADER_LENGTH	mb2_header_end - mb2_header_start
	.align  L4UTIL_MB2_TAG_ALIGN
mb2_header_start:
	.long   L4UTIL_MB2_MAGIC
	.long   L4UTIL_MB2_ARCH_I386
	.long   MB2_HEADER_LENGTH
	.long   0 - (L4UTIL_MB2_MAGIC + L4UTIL_MB2_ARCH_I386 + MB2_HEADER_LENGTH)

	.align  L4UTIL_MB2_TAG_ALIGN
info_req_tag_start:
	.word   L4UTIL_MB2_INFO_REQUEST_HEADER_TAG
	.word   L4UTIL_MB2_TAG_FLAG_REQUIRED
	.long   info_req_tag_end - info_req_tag_start
	.long   L4UTIL_MB2_BOOT_CMDLINE_INFO_TAG
	.long   L4UTIL_MB2_MODULE_INFO_TAG
	.long   L4UTIL_MB2_MEMORY_MAP_INFO_TAG
	.long   L4UTIL_MB2_RSDP_OLD_INFO_TAG
	.long   L4UTIL_MB2_RSDP_NEW_INFO_TAG
info_req_tag_end:

	.align  L4UTIL_MB2_TAG_ALIGN
entry_address_tag_start:
	.word   L4UTIL_MB2_ENTRY_ADDRESS_HEADER_TAG
	.word   L4UTIL_MB2_TAG_FLAG_REQUIRED
	.long   entry_address_tag_end - entry_address_tag_start
	.long   _mb2_start
entry_address_tag_end:

	.align  L4UTIL_MB2_TAG_ALIGN
terminator_tag_start:
	.word   L4UTIL_MB2_TERMINATOR_HEADER_TAG
	.word   L4UTIL_MB2_TAG_FLAG_REQUIRED
	.long   terminator_tag_end - terminator_tag_start
terminator_tag_end:
mb2_header_end:

_mb2_start:
	leal    _stack, %esp
	pushl   %ebx                    /* mb2i */
	call    _multiboot2_to_multiboot
	movl    $L4UTIL_MB_VALID_ASM, %eax
	jmp     _start                  /* jump to the original entry point */
