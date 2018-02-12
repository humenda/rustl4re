/*
 * (c) 2009 Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/* -*- c -*- */

	.globl	_start
	.p2align 4
_start:
	leal	_stack,%esp

	pushl	%esi			/* ptr to real mode */
	pushl	%eax
	pushl	%ebx
	pushl	$0			/* no return address */
	jmp	bootstrap

        .align  4

	.p2align(2), 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

	.bss
	.space	8192
.globl _stack
_stack:
