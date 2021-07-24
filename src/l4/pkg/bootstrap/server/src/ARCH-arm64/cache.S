/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2014-2020 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

.macro set_sctlr sctlr_reg, tmp
	mrs	\tmp, \sctlr_reg
	bic	\tmp, \tmp, #0x0001   // disable MMU
	bic	\tmp, \tmp, #0x0004   // disable data cache
	bic	\tmp, \tmp, #0x1000   // disable instruction cache
	msr	\sctlr_reg, \tmp
.endm


.text

.global armv8_disable_mmu
.type   armv8_disable_mmu, %function
armv8_disable_mmu:
    /*
     * First clean the entire data or unified cache to the point of coherency.
     * Taken literally from the ARMv8 Architecture Reference Manual.
     */
    MRS   X0, CLIDR_EL1
    AND   W3, W0, #0x07000000     // get 2 x level of coherency
    LSR   W3, W3, #23
    CBZ   W3, 5f
    MOV   W10, #0                 // W10 = 2 x cache level
    MOV   W8, #1                  // W8 = constant 0b1
1:  ADD   W2, W10, W10, LSR #1    // calculate 3 x cache level
    LSR   W1, W0, W2              // extract 3-bit cache type for this level
    AND   W1, W1, #0x7
    CMP   W1, #2
    B.LT  4f                      // no data or unified cache at this level
    MSR   CSSELR_EL1, X10         // select this cache level
    ISB                           // sync change of CSSELR
    MRS   X1, CCSIDR_EL1          // read CCSIDR
    AND   W2, W1, #7              // W2 = log2(linelen)-4
    ADD   W2, W2, #4              // W2 = log2(linelen)
    UBFX  W4, W1, #3, #10         // W4 = max way number, right aligned
    CLZ   W5, W4                  // W5 = 32-log2(ways), bit position of way in DC operand
    LSL   W9, W4, W5              // W9 = max way number, aligned to position in DC operand
    LSL   W16, W8, W5             // W16 = amount to decrement way number per iteration
2:  UBFX  W7, W1, #13, #15        // W7 = max set number, right aligned
    LSL   W7, W7, W2              // W7 = max set number, aligned to position in DC operand
    LSL   W17, W8, W2             // W17 = amount to decrement set number per iteration
3:  ORR   W11, W10, W9            // W11 = combine way number and cache number ...
    ORR   W11, W11, W7            // ... and set number for DC operand
    DC    CSW, X11                // do data cache clean by set and way
    SUBS  W7, W7, W17             // decrement set number
    B.GE  3b
    SUBS  X9, X9, X16             // decrement way number
    B.GE  2b
4:  ADD   W10, W10, #2            // increment 2 x cache level
    CMP   W3, W10
    B.GT  1b
    DSB  SY                         // ensure completion of the clean on all processors
5:

    /*
     * Invalidate entire instruction cache.
     */
    ic    iallu
    dsb   sy
    isb

    /*
     * Disable all caches.
     */
    mrs   x2, CurrentEL
    ubfx  x2, x2, #2, #2
    cmp   x2, #3
    b.eq  .Lsctlr_el3
    cmp   x2, #2
    b.eq  .Lsctlr_el2

    set_sctlr SCTLR_EL1, x2
    b .Lsctlr_done
.Lsctlr_el2:
    set_sctlr SCTLR_EL2, x2
    b .Lsctlr_done
.Lsctlr_el3:
    set_sctlr SCTLR_EL3, x2
.Lsctlr_done:

    isb
    ret

    .size   armv8_disable_mmu, .-armv8_disable_mmu

