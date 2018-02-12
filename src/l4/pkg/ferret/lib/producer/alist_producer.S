/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
__ferret_kern_lib_page = 0xef7ff000
.hidden __ferret_kern_lib_page

	.macro	kern_lib_page_entry entry,offset
\entry = __ferret_kern_lib_page + \offset
.global \entry
.hidden \entry
	.endm

	kern_lib_page_entry	__ferret_alist_post, 0x0
