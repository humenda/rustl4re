/*
 * (c) 2009 Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#define GATE_INITTAB_BEGIN(name)	\
	.text	1			;\
	.globl	name			;\
name:					;\
	.text

#define	GATE_ENTRY(n,entry,type)	\
	.text	1			;\
	.long	entry			;\
	.word	n			;\
	.word	type			;\
	.text

#define GATE_INITTAB_END		\
	.text	1			;\
	.long	0			;\
	.text

#define	EXCEPTION(n,name)		\
	GATE_ENTRY(n,name,0x0e)		;\
name:					;\
	pushl	$(0)			;\
	pushl	$(n)			;\
	jmp	alltraps

#define	EXCEP_USR(n,name)		\
	GATE_ENTRY(n,name,0x6e)		;\
name:					;\
	pushl	$(0)			;\
	pushl	$(n)			;\
	jmp	alltraps

#define	EXCEP_ERR(n,name)		\
	GATE_ENTRY(n,name,0x0e)		;\
name:					;\
	pushl	$(n)			;\
	jmp	alltraps


GATE_INITTAB_BEGIN(boot_idt_inittab)

EXCEPTION(0x00,t_zero_div)
EXCEPTION(0x01,t_debug)
EXCEPTION(0x02,t_nmi)
EXCEP_USR(0x03,t_int3)
EXCEP_USR(0x04,t_into)
EXCEP_USR(0x05,t_bounds)
EXCEPTION(0x06,t_invop)
EXCEPTION(0x07,t_nofpu)
GATE_ENTRY(0x08,0x20,0x05)
EXCEPTION(0x09,a_fpu_over)
EXCEP_ERR(0x0a,a_inv_tss)
EXCEP_ERR(0x0b,t_segnp)
EXCEP_ERR(0x0c,t_stack_fault)
EXCEP_ERR(0x0d,t_gen_prot)
EXCEP_ERR(0x0e,t_page_fault)
EXCEPTION(0x0f,t_trap_0f)
EXCEPTION(0x10,t_fpu_err)
EXCEPTION(0x11,t_trap_11)
EXCEPTION(0x12,t_trap_12)
EXCEPTION(0x13,t_trap_13)
EXCEPTION(0x14,t_trap_14)
EXCEPTION(0x15,t_trap_15)
EXCEPTION(0x16,t_trap_16)
EXCEPTION(0x17,t_trap_17)
EXCEPTION(0x18,t_trap_18)
EXCEPTION(0x19,t_trap_19)
EXCEPTION(0x1a,t_trap_1a)
EXCEPTION(0x1b,t_trap_1b)
EXCEPTION(0x1c,t_trap_1c)
EXCEPTION(0x1d,t_trap_1d)
EXCEPTION(0x1e,t_trap_1e)
EXCEPTION(0x1f,t_trap_1f)

GATE_INITTAB_END

alltraps:
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	movl	%ss,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%esp,%eax
	pushl	%eax
	call	trap_dump_panic

