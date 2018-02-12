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

#define MIMIC_A_VMLINUZ

.section .text.init,#alloc,#execinstr
.type _start,#function
.globl _start
_start:
/* Some bootloaders like it this way, for others it won't harm */
#ifdef MIMIC_A_VMLINUZ
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	b 10f
	.word 0x016f2818
	.word _start
	.word _module_data_end
10:
#endif
	/*
	 * It might be the case that we're not run at the position where we
	 * have been linked to. If this is the case we copy ourselves to the
	 * position we're linked to.
	 */
	adr	r4, run		/* Running version */
	ldr	r5, .LCrun	/* supposed to be version */
	cmp	r4, r5		/* If equal ... */
	beq	run		/* ... go to run */

	/* Disable caches as we're moving code around */
	mcr p15, 0, r3, c7, c5, 0 /* ICIALLU */
	mrc p15, 0, r0, c1, c0
	bic r0, #0x0004
	bic r0, #0x1000
	mcr p15, 0, r0, c1, c0

	/* Figure how to move */
        ldr     r7, .LCend_bin
	subs	r8, r5, r4	/* r8 is the distance between the blocks */
	bpl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	r9, r5			/* r9: run address */
	ldr	r0, .LCstart_bin
	ldr	r3, 3f
	str	r3, [r0], #4
	ldr	r3, 32f
	str	r3, [r0], #4
	ldr	r3, 33f
	str	r3, [r0], #4
	ldr	r3, 34f
	str	r3, [r0], #4
	ldr	r3, 35f
	str	r3, [r0], #4
	ldr	pc, .LCstart_bin

3:	ldr	r6, [r4], #4
32:	str	r6, [r5], #4
33:	cmp	r5, r7
34:	blt	3b
35:	mov	pc, r9

	/* Copy behind, copy backwards */
move_behind:
	sub	r8, r7, r8	/* r8 points to the end of source image */
3:	ldr	r6, [r8, #-4]!	/* Take bytes */
	str	r6, [r7, #-4]!	/* Put bytes */
	cmp	r5, r7
	blt	3b
	ldr	pc, .LCrun


.LCrun:       .word run
.LCstart_bin: .word _start
.LCend_bin:   .word _module_data_end

run:
	mov	r3, #0x1000
	sub	r3, r3, #1                 /* r3 == 0xfff */
	mrc	p15, 0, r0, c0, c0, 0      /* Main ID */
	lsr	r0, #4
	and	r0, r0, r3

	/* Check for processors that understand CPU ID */
	mov	r9, #0xb00
	orr 	r9, #0x002
	cmp	r0, r9
	beq	do_cpuid

	mov	r3, #0xf00
	and	r0, r3

	mov	r9, #0xc00
	cmp	r0, r9
	beq	do_cpuid

	mov	r9, #0xd00
	cmp	r0, r9
	bne	do_bootstrap               /* None matched, normal startup */

do_cpuid:
	mrc	p15, 0, r0, c0, c0, 5      /* CPU ID */
	and     r0, r0, #0xf               /* CPU id */
	cmp	r0, #0                     /* CPU0 continues with bootstrap */
	beq	do_bootstrap	

/* CPU1+ wait for bootup */

	// I-cache on
	mrc p15, 0, r0, c1, c0, 0
	orr r0, r0, #(1 << 12)
	mcr p15, 0, r0, c1, c0, 0

	// IRQs off, SVC
	mrs r0, cpsr
	orr r0, #0xd3
	msr cpsr_c, r0

#if defined(PLATFORM_TYPE_rv) || defined(PLATFORM_TYPE_rv_pbx) || \
    defined(PLATFORM_TYPE_rv_vexpress) || defined(PLATFORM_TYPE_rv_vexpress_a15)

	// enable GIC CPU interface + prio mask for IRQs
	mrc	p15, 0, r0, c0, c0, 0    // r0: cpu-id
	and	r0, r0, #0x70
	cmp	r0, #0x70

	// get board ID and retrieve MPCore-base from table
	movne	r7, #0x10000000
	moveq	r7, #0x00010000
	orreq	r7, #0x1c000000
	ldr	r5, [r7]
	bic	r5, #0xff

	adr	r6, .Lboard_data_table
2:
	ldr	r4, [r6]
	cmp	r4, #0
	beq	3f
	cmp	r4, r5
	bne	4f

	ldr	r3, [r6, #4]
	cmp	r0, r3
	beq	3f
	cmp	r3, #0
	beq	3f
4:	add	r6, #12
	b	2b

3:
	ldr	r4, [r6, #8]
	mov	r0, #0x1
	str	r0, [r4, #0]
	mov	r0, #0xf0
	str	r0, [r4, #4]

1:
	ldr	r6, [r7, #0x30]
	cmp	r6, #0
	movne	pc, r6
	.inst	0xe320f003 /* wfi */
	ldr	r0, [r4, #12]
	str	r0, [r4, #16]
	b	1b

.Lboard_data_table:
	/* VExpress */
	.word	0x1190f500 /* Board ID */
	.word	0x00000070 /* CPU ID */
	.word	0x2c002000 /* MPCore base */

	.word	0x1190f500 /* Board ID */
	.word	0x00000000 /* CPU ID */
	.word	0x1e000100 /* MPCore base */

	/* Default value (with #0) must come last! */
	/* Realview */
	.word	0
	.word	0
	.word	0x1f000100

#else
1:	.word 0xe320f003 /* wfi */
	b	1b
#endif

do_bootstrap:
	ldr	sp, .LCstack

	mov	r0, r2                     /* ATAG pointer */
	bl	__main
1:	b	1b

.LCstack: .word crt0_stack_high

.section ".bss"

	.global crt0_stack_low
	.align 3
crt0_stack_low:
	.space	8192
	.global crt0_stack_high
crt0_stack_high:
