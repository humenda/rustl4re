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
	mcr p15, 0, r6, c7, c5, 0 /* ICIALLU */
	mrc p15, 0, r6, c1, c0
	bic r6, #0x0004
	bic r6, #0x1000
	mcr p15, 0, r6, c1, c0

	/* Figure how to move */
        ldr     r7, crt_end_bin_lsb
	subs	r8, r5, r4	/* r8 is the distance between the blocks */
	bpl	move_behind

	/* Copy before, copy forwards */
	/* First, copy our copy loop to the very beginning to avoid code
	 * overwrites */
	mov	r9, r5			/* r9: run address */
	ldr	r10, .LCstart_bin
	ldr	r6, 3f
	str	r6, [r10], #4
	ldr	r6, 32f
	str	r6, [r10], #4
	ldr	r6, 33f
	str	r6, [r10], #4
	ldr	r6, 34f
	str	r6, [r10], #4
	ldr	r6, 35f
	str	r6, [r10], #4
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
crt_end_bin:     .word 0
crt_end_bin_lsb: .word _module_data_end;

	.global cpu_bootup_code_start
cpu_bootup_code_start:
run:
	mov	r5, #0x1000
	sub	r5, r5, #1                 /* r5 == 0xfff */
	mrc	p15, 0, r6, c0, c0, 0      /* Main ID */
	lsr	r6, #4
	and	r6, r6, r5

	/* Check for processors that understand CPU ID */
	mov	r9, #0xb00
	orr 	r9, #0x002
	cmp	r6, r9
	beq	do_cpuid

	mov	r5, #0xf00
	and	r6, r5

	mov	r9, #0xc00
	cmp	r6, r9
	beq	do_cpuid

	mov	r9, #0xd00
	cmp	r6, r9
	bne	do_bootstrap               /* None matched, normal startup */

do_cpuid:
	mrc	p15, 0, r4, c0, c0, 5      /* CPU ID */
	and     r4, r4, #0xf               /* CPU id */
	cmp	r4, #0                     /* CPU0 continues with bootstrap */
	beq	do_bootstrap	

/* CPU1+ wait for bootup */

	// I-cache on
	mrc p15, 0, r4, c1, c0, 0
	orr r4, r4, #(1 << 12)
	mcr p15, 0, r4, c1, c0, 0

	// IRQs off, SVC
	mrs r4, cpsr
	orr r4, #0xd3
	msr cpsr_c, r4

#if defined(PLATFORM_TYPE_rv) || defined(PLATFORM_TYPE_rv_pbx) || \
    defined(PLATFORM_TYPE_rv_vexpress) || defined(PLATFORM_TYPE_rv_vexpress_a15)

	// enable GIC CPU interface + prio mask for IRQs
	mrc	p15, 0, r8, c0, c0, 0    // r8: cpu-id
	and	r8, r8, #0x70
	cmp	r8, #0x70

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

	ldr	r9, [r6, #4]
	cmp	r8, r9
	beq	3f
	cmp	r9, #0
	beq	3f
4:	add	r6, #12
	b	2b

3:
	ldr	r4, [r6, #8]
	mov	r8, #0x1
	str	r8, [r4, #0]
	mov	r8, #0xf0
	str	r8, [r4, #4]

1:
	ldr	r6, [r7, #0x30]
	cmp	r6, #0
	movne	pc, r6
	.inst	0xe320f003 /* wfi */
	ldr	r8, [r4, #12]
	str	r8, [r4, #16]
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
	.global cpu_bootup_code_end
cpu_bootup_code_end:

do_bootstrap:
#ifdef __ARM_FP
	/* Prevent triggering 'undefined instruction' exceptions by enabling
         * the FPU. Newer gcc versions implicitly generate FP instructions. */
	mrc	p15, 0, r8, c1, c0, 2     /* read CPACR */
	orr	r8, #(0xf << 20)          /* cp10 + cp11 (FPU) */
	mcr	p15, 0, r8, c1, c0, 2     /* try to enable */
	mrc	p15, 0, r8, c1, c0, 2     /* read CPACR */
	and	r8, #(0xf << 20)
	cmp	r8, #(0xf << 20)          /* verify enabled */
	bne	no_fpu
	vmrs	r8, fpexc
	orr	r8, r8, #(0x1 << 30)      /* enable FP unit */
	vmsr	fpexc, r8

no_fpu:
#endif

	ldr	sp, .LCstack
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
