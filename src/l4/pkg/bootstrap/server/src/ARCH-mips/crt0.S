/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License 2.  See the file "COPYING-GPL-2" in the main directory of this
 * archive for more details.
 *
 * Copyright (C) 2013 Imagination Technologies Ltd.
 * Author: Prajna Dasgupta <prajna@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

/* -*- c -*- */

#include <sys/asm.h>

/* This routine makes changes to the instruction stream effective to the hardware.
 * It should be called after the instruction stream is written.
 * On return, the new instructions are effective.
 * Inputs:
 * a0 = Start address of new instruction stream
 * a1 = Size, in bytes, of new instruction stream
 */

#define HW_SYNCI_Step       $1

.text
.globl syncICache;
.ent  syncICache, 0;
syncICache: ;
.set push
.set noreorder
	beq     $a1, $0, 20f
	  nop

	# figure out the synci step size the hard way, as rdhwr
	# might not work in kernel mode ?
	# so read the data and isns cache line size, take the smaller
	# one and use this as step size
	mfc0	$v1, $16, 1
	ext	$v0, $v1, 10, 3  # D-cache line size
	beq	$v0, $zero, 20f  # if this is zero (no D-cache) we skip all
	  ext	$v1, $v1, 19, 3  # I-cache line size
	beq	$v1, $zero, 1f   # if zero (no I-cache) we use the DL
	  nop
	sltu	$t0, $v0, $v1    # if DL < IL use DL else IL
	beq	$t0, $zero, 1f
	  nop
	move	$v0, $v1
1:
	li	$v1, 2        # size is 2 << min(DL, IL)
	sllv	$v0, $v1, $v0

	# run the loop
10:
	synci   0($a0)

	addu    $a0, $a0, $v0
	sltu    $v1, $a0, $a1
	bne     $v1, $0, 10b
	  nop
	sync
20:
	jr.hb   $ra
	  nop
.set pop
.end  syncICache

.section .text.init, "ax"
.type _start, @function
.globl _start
_start:
.set push
.set noreorder
	/* initialize GP */
	.cpload $25
	SETUP_GPX64($25, $0)

#if defined(PLATFORM_TYPE_baikal_t) || defined(PLATFORM_TYPE_ci40)
	// Fix invalid setting of CCA by boot loader
	mfc0  $8, $16, 0
	li    $9, 5
	ins   $8, $9, 0, 3
	mtc0  $8, $16, 0
#endif

	/* for bootstrap, map the first 0x20000000 of memory 1:1 in USEG address space */
	li    $8, 0x00000000
	mtc0  $8, $0, 0          // mtc0 t0, c0_index
	li    $8, 0x1ffff800     // li   t0, 0x1ffff800
	mtc0  $8, $5, 0          // mtc0 t0, c0_pagemask (256MB)
	li    $8, 0x0000001F     // even page @0MB, cached, dirty, valid, global
	mtc0  $8, $2, 0          // mtc0 t0, c0_entrylo0
	li    $8, 0x0080001F     // odd page @256MB, cached, dirty, valid, global
	mtc0  $8, $3, 0          // mtc0 t0, c0_entrylo1
	li    $8, 0x00000000
	mtc0  $8, $10, 0         // mtc0 t0, c0_entryhi (1:1 mapping)
	ehb
	tlbwi
	ehb

	mfc0  $8, $12, 0         // mfc0 t0, c0_status
	li    $9, 0x20000000     // set ST0_CU1 to enable FPU
	or    $8, $8, $9
	mtc0  $8, $12, 0         // mtc0 t0, c0_status
	ehb

	mfc0  $8, $15, 1         // Get CPU ID
	andi  $8, $8, 0x3ff
	beqz  $8, 7f
	nop
6:	wait
	b     6b
	nop
7:


	PTR_LA $gp, _gp
	PTR_LA    $29, crt0_stack_high
	PTR_SUBU  $29, (NARGSAVE * SZREG)
	PTR_LA    $25, __main        // jump to USEG address space
	jr    $25
	nop
1:
	b   1b
	nop
.set pop

.section ".bss", "aw"
.align 12
.global crt0_stack_low
crt0_stack_low:
	.space 4096
.global crt0_stack_high
crt0_stack_high:
