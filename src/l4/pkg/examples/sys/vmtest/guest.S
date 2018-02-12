/*
 * Copyright (C) 2014-2015 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
.section .bss
.p2align 12
.globl is_vmx
is_vmx:
.long 0x0

.macro CALLVMM edx
  push %eax
  push %edx
  movl \edx, %edx
  movl $(((vmcall) - (guestcode32)) + 0x1000), %eax
  call *%eax // vmcall is on same page as guestcode
  pop %edx
  pop %eax
.endm

/**
 * Guest code.
 *
 * This code runs inside the virtual machine.
 */
.section .text
.code32
.globl guestcode32
.p2align 12
guestcode32:
  // first, prove that code runs in the VM
  inc %eax
  CALLVMM edx=$0xdeadbeef

  movl $0, %ecx
  // test divide by zero exception, should be handled by guest (irq_dz)
  // this ensures that the guest IDT works
  movl $0, %eax
  movl $0, %ebx
  divl %ebx

  // test that the GDT works
  // data segment
  movl $0x10, %eax
  movl %eax, %ds

  // stack segment
  movl $0x28, %eax
  movl %eax, %ss // set stack segment
  // test stack
  pushl %eax
  popl %eax

  // switch to ring3 to trigger #ac
  mov  %esp, %eax
  push $0x23 //user data segment with bottom 2 bits set for ring 3
  push %eax  //push our current stack
  pushf
  push $0x1b; //user code segment with bottom 2 bits set for ring 3
  push $(((usermode) - (guestcode32)) + 0x1000)
  iret

.globl usermode
usermode:
  // set data segment
  movl $0x23, %eax
  movl %eax,  %ds

  // set proper stack segment
  movl $0x33, %eax
  movl %eax, %ss

  // now switch to user stack...
  movl $0x5000, %eax
  movl %eax, %esp

  // now, test stack
  pushl %eax
  popl  %eax

  // trigger alignment check exception
  sub $3, %esp
  push %eax
  popl %eax

  // never reach this
  ret

.globl guestcode64
guestcode64:
  CALLVMM edx=$0x4242

  // never reach this
  ret

  // code left here for documentation
  // trigger debug exception
  movl $(((debug) - (guestcode32)) + 0x1000), %eax
  movl %eax, %dr0 // this will be intercepted in kvm

debug:
  nop
  nop

// function vmcall, uses edx to indicate reason for vmcall
.globl vmcall
.type  vmcall, @function
vmcall:
  cmpl $0, (0x3000) // is_vmx
  jne _vmx
  vmmcall
  jmp _out
_vmx:
  vmcall
_out:
  ret

.globl interrupt_handlers
.p2align 12
interrupt_handlers:
  nop
.globl irq_dz
// #dz interrupt handler - just advances the eip over the offending instruction
irq_dz:
  CALLVMM edx=$0x42
  addl $2, (%esp) // move eip
  ret
.globl irq_db
irq_db:
  CALLVMM edx=$0x44
  ret
.globl irq_bp
irq_bp:
  CALLVMM edx=$0x48
  ret
.globl irq_ac
irq_ac:
  CALLVMM edx=$0x50
  add $4, %esp
  iret

