/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/* -*- c -*- */

.macro set_sctlr sctlr_reg, tmp
	mrs	\tmp, \sctlr_reg
	bic	\tmp, \tmp, #0x0004
	bic	\tmp, \tmp, #0x1000
	msr	\sctlr_reg, \tmp
.endm

.section .text.init,#alloc,#execinstr
.type _start,#function
.globl _start
_start:
	/*
	 * It might be the case that we're not run at the position where we
	 * have been linked to. If this is the case we copy ourselves to the
	 * position we're linked to.
	 */
	adr	x4, run		/* Running version */
	ldr	x5, .LCrun	/* supposed to be version */
	cmp	x4, x5		/* If equal ... */
	b.eq	run		/* ... go to run */

	/* Disable caches as we're moving code around */
	ic	iallu
	mrs	x2, CurrentEL
	ubfx	x2, x2, #2, #2
	cmp	x2, #3
	b.eq	.Lsctlr_el3
	cmp	x2, #2
	b.eq	.Lsctlr_el2

	set_sctlr SCTLR_EL1, x2
	b .Lsctlr_done
.Lsctlr_el2:
	set_sctlr SCTLR_EL2, x2
	b .Lsctlr_done
.Lsctlr_el3:
	set_sctlr SCTLR_EL3, x2
.Lsctlr_done:

	/* Figure how to move */
        ldr     x7, .LCend_bin
	subs	x8, x5, x4	/* x8 is the distance between the blocks */
	b.pl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	x9, x5			/* x9: run address */
	ldr	x2, .LCstart_bin
	ldr	x3, 3f
	str	x3, [x2], #8
	ldr	x3, 33f
	str	x3, [x2], #8
	ldr	x3, 35f
	str	x3, [x2], #8
	ldr	x10, .LCstart_bin
	br	x10

3:	ldr	x6, [x4], #8
	str	x6, [x5], #8
33:	cmp	x5, x7
	b.lt	3b
35:	br	x9
	nop

	/* Copy behind, copy backwards */
move_behind:
	sub	x8, x7, x8	/* x8 points to the end of source image */
3:	ldr	x6, [x8, #-8]!	/* Take bytes */
	str	x6, [x7, #-8]!	/* Put bytes */
	cmp	x5, x7
	b.lt	3b
	ldr	x10, .LCrun
	br	x10


	.align 3
.LCrun:       .8byte run
.LCstart_bin: .8byte _start
.LCend_bin:   .8byte _module_data_end

run:
	mrs	x9, MPIDR_EL1              /* CPU ID */
	and	x9, x9, #0xf               /* CPU id */
	cmp	x9, #0                     /* CPU0 continues with bootstrap */
	beq	do_bootstrap

1:	ldr	x9, mp_launch_spin_addr
	cmp	x9, #0
	beq	2f                         /* Not yet */
	ldr	x9, [x9]
	cmp	x9, #0
	beq	2f                         /* Still not set */
	blr	x9
2:	wfi
	b	1b

do_bootstrap:
	adr	x9, crt0_stack_high
	mov	sp, x9

	mrs	x8, cpacr_el1
	orr	x8, x8, #0x300000 // fpen
	msr	cpacr_el1, x8


	bl	__main
1:	b	1b

.section ".bss"

	.global crt0_stack_low
	.align 4
crt0_stack_low:
	.space	8192
	.global crt0_stack_high
crt0_stack_high:

	.global mp_launch_spin_addr
mp_launch_spin_addr:
	.space 8
