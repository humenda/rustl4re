
#include <linux/kernel.h>
#include <linux/sched/task_stack.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/tick.h>
#include <linux/kprobes.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

#include <asm/processor.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/unistd.h>
#include <asm/traps.h>
#include <asm/fpu/internal.h>
#include <asm/switch_to.h>
#include <asm/kdebug.h>

#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/utcb.h>
#include <l4/sys/segment.h>
#include <l4/sys/ktrace.h>
#include <l4/util/util.h>
#include <l4/log/log.h>

#include <asm/l4lxapi/task.h>
#include <asm/l4lxapi/thread.h>
#include <asm/l4lxapi/memory.h>
#include <asm/api/macros.h>

#include <asm/generic/dispatch.h>
#include <asm/generic/task.h>
#include <asm/generic/memory.h>
#include <asm/generic/setup.h>
#include <asm/generic/ioremap.h>
#include <asm/generic/hybrid.h>
#include <asm/generic/syscall_guard.h>
#include <asm/generic/stats.h>
#include <asm/generic/smp.h>

#include <asm/l4x/exception.h>
#include <asm/l4x/fpu.h>
#include <asm/l4x/l4_syscalls.h>
#include <asm/l4x/lx_syscalls.h>
#include <asm/l4x/utcb.h>
#include <asm/l4x/signal.h>
#include <asm/l4x/entry-common.h>

#if 0
#define TBUF_LOG_IDLE(x)        TBUF_DO_IT(x)
#define TBUF_LOG_WAKEUP_IDLE(x)	TBUF_DO_IT(x)
#define TBUF_LOG_USER_PF(x)     TBUF_DO_IT(x)
#define TBUF_LOG_INT80(x)       TBUF_DO_IT(x)
#define TBUF_LOG_EXCP(x)        TBUF_DO_IT(x)
#define TBUF_LOG_START(x)       TBUF_DO_IT(x)
#define TBUF_LOG_SUSP_PUSH(x)   TBUF_DO_IT(x)
#define TBUF_LOG_DSP_IPC_IN(x)  TBUF_DO_IT(x)
#define TBUF_LOG_DSP_IPC_OUT(x) TBUF_DO_IT(x)
#define TBUF_LOG_SUSPEND(x)     TBUF_DO_IT(x)
#define TBUF_LOG_HYB_BEGIN(x)   TBUF_DO_IT(x)
#define TBUF_LOG_HYB_RETURN(x)  TBUF_DO_IT(x)

#else

#define TBUF_LOG_IDLE(x)
#define TBUF_LOG_WAKEUP_IDLE(x)
#define TBUF_LOG_USER_PF(x)
#define TBUF_LOG_INT80(x)
#define TBUF_LOG_EXCP(x)
#define TBUF_LOG_START(x)
#define TBUF_LOG_SUSP_PUSH(x)
#define TBUF_LOG_DSP_IPC_IN(x)
#define TBUF_LOG_DSP_IPC_OUT(x)
#define TBUF_LOG_SUSPEND(x)
#define TBUF_LOG_HYB_BEGIN(x)
#define TBUF_LOG_HYB_RETURN(x)

#endif

// In arch/x86/entry/common.c
// Better: Use do_syscall_64 and do_syscall_32_irqs_on
//long syscall_trace_enter(struct pt_regs *regs);

#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
// entry/common.c
void do_syscall_32_irqs_on(struct pt_regs *regs);
#endif

#ifdef CONFIG_X86_32
#include "dispatch_32.c"
#endif
#ifdef CONFIG_X86_64
#include "dispatch_64.c"
#endif

static inline int l4x_msgtag_fpu(unsigned cpu)
{
	return L4_MSGTAG_TRANSFER_FPU;
}

static inline int l4x_msgtag_copy_ureg(l4_utcb_t *u)
{
	return 0;
}

static inline int l4x_is_triggered_exception_from_val(l4_umword_t val)
{
	return val == 0xff;
}

static inline unsigned long regs_pc(struct task_struct *p)
{
	return task_pt_regs(p)->ip;
}

static inline unsigned long regs_sp(struct task_struct *p)
{
	return task_pt_regs(p)->sp;
}

static inline void l4x_arch_task_setup(struct thread_struct *t)
{
	load_TLS(t, smp_processor_id());
}

static inline void l4x_arch_do_syscall_trace(struct task_struct *p)
{
}

static inline int l4x_hybrid_check_after_syscall(l4_utcb_t *utcb)
{
	l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	return (exc->trapno == 0xd /* after L4 syscall */
	        && l4x_l4syscall_get_nr(exc->err, exc->ip) != -1
	        && (exc->err & 4))
	       || (exc->trapno == 0xff /* L4 syscall exr'd */
	           && exc->err == 0);
}

#ifdef CONFIG_L4_VCPU
static inline void l4x_arch_task_start_setup(l4_vcpu_state_t *v,
		                             struct task_struct *p, l4_cap_idx_t task_cap)
#else
static inline void l4x_arch_task_start_setup(struct task_struct *p, l4_cap_idx_t task_cap)
#endif
{
	// - remember GS in FS so that programs can find their UTCB
	//   libl4sys-l4x.a uses %fs to get the UTCB address
	// - do not set GS because glibc does not seem to like if gs is not 0
	// - only do this if this is the first usage of the L4 thread in
	//   this task, otherwise gs will have the glibc-gs
	// - ensure this by checking if the segment is one of the user ones or
	//   another one (then it's the utcb one)
#ifdef CONFIG_X86_32
#ifdef CONFIG_L4_VCPU
	unsigned int gs = v->r.gs;
#else
	unsigned int gs = l4_utcb_exc()->gs;
#endif
	unsigned int val = (gs & 0xffff) >> 3;
	if (   val < l4x_x86_fiasco_gdt_entry_offset
	    || val > l4x_x86_fiasco_gdt_entry_offset + 3)
		task_pt_regs(p)->fs = gs;

#ifdef CONFIG_MODIFY_LDT_SYSCALL
	/* Setup LDTs */
	if (p->mm && p->mm->context.ldt) {
		unsigned i;
		L4XV_V(f);
		L4XV_L(f);
		for (i = 0; i < p->mm->context.ldt->nr_entries;
		     i += L4_TASK_LDT_X86_MAX_ENTRIES) {
			unsigned sz = p->mm->context.ldt->nr_entries- i;
			int r;
			if (sz > L4_TASK_LDT_X86_MAX_ENTRIES)
				sz = L4_TASK_LDT_X86_MAX_ENTRIES;
			r = fiasco_ldt_set(task_cap, p->mm->context.ldt,
			                   sz, i, l4_utcb());
			if (r)
				LOG_printf("fiasco_ldt_set(%d, %d): failed %d\n",
				           sz, i, r);

		}
		L4XV_U(f);
	}
#endif
#endif
}

static inline l4_umword_t l4x_l4pfa(struct thread_struct *t)
{
	return t->cr2;
}

static inline int l4x_iswrpf(unsigned long error_code)
{
	return error_code & 2;
}

static inline int l4x_ispf_v(l4_vcpu_state_t *v)
{
	return v->r.trapno == 14;
}

static inline int l4x_ispf_t(struct thread_struct *t)
{
	return t->trap_nr == 14;
}

asmlinkage void ret_from_fork(void) __asm__("ret_from_fork");

#ifndef CONFIG_L4_VCPU
void l4x_idle(void);
#endif

int  l4x_deliver_signal(int exception_nr);

#ifndef CONFIG_THREAD_INFO_IN_TASK
#error CONFIG_THREAD_INFO_IN_TASK must be defined here
#endif
DEFINE_PER_CPU(struct thread_info *, l4x_current_ti) = (struct thread_info *)&init_task;
DEFINE_PER_CPU(struct thread_info *, l4x_current_proc_run);
#ifndef CONFIG_L4_VCPU
static DEFINE_PER_CPU(unsigned, utcb_snd_size);
#endif

static inline void l4x_pte_add_access_flag(pte_t *ptep)
{
	ptep->pte |= _PAGE_ACCESSED;
}

static inline void l4x_pte_add_access_and_dirty_flags(pte_t *ptep)
{
	ptep->pte |= _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_SOFT_DIRTY;
}

#ifdef CONFIG_L4_VCPU
static inline void
state_to_vcpu(l4_vcpu_state_t *vcpu, struct pt_regs *regs,
              struct task_struct *p)
{
	ptregs_to_vcpu(vcpu, regs);
}

static inline void vcpu_to_thread_struct(l4_vcpu_state_t *v,
                                         struct thread_struct *t)
{
#ifdef CONFIG_X86_32
	t->gs         = v->r.gs;
#endif
	t->trap_nr    = v->r.trapno;
	t->error_code = v->r.err;
	t->cr2        = v->r.pfa;
}

static inline void thread_struct_to_vcpu(l4_vcpu_state_t *v,
                                         struct thread_struct *t)
{
#ifdef CONFIG_X86_32
	v->r.gs = t->gs;
#endif
}
#else
static inline void utcb_to_thread_struct(l4_utcb_t *utcb,
                                         struct task_struct *p,
                                         struct thread_struct *t)
{
	l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	utcb_exc_to_ptregs(exc, task_pt_regs(p));
	task_pt_regs(p)->cs = USER_RPL;
#ifdef CONFIG_X86_32
	t->gs         = exc->gs;
#else
	t->fsindex    = exc->fs;
	t->gsindex    = exc->gs;
	t->fsbase     = exc->fs_base;
	t->gsbase     = exc->gs_base;
	t->es         = exc->es;
	t->ds         = exc->ds;
#endif
	t->trap_nr    = exc->trapno;
	t->error_code = exc->err;
	t->cr2        = exc->pfa;
}
#endif

static inline void thread_struct_to_utcb(struct task_struct *p,
                                         struct thread_struct *t,
                                         struct pt_regs *regs,
                                         l4_utcb_t *utcb,
                                         unsigned int send_size)
{
	l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	ptregs_to_utcb_exc(regs, exc);
#ifdef CONFIG_X86_32
	exc->gs      = t->gs;
#else
	exc->fs      = t->fsindex;
	exc->gs      = t->gsindex;
	exc->es      = L4X_SEG_CUR(es);
	exc->ds      = L4X_SEG_CUR(ds);
	exc->fs_base = L4X_SEG_CUR(fs_base);
	exc->gs_base = L4X_SEG_CUR(gs_base);
#endif
#ifndef CONFIG_L4_VCPU
	per_cpu(utcb_snd_size, smp_processor_id()) = send_size;
#endif
}

#ifndef CONFIG_L4_VCPU
static int l4x_hybrid_begin(struct task_struct *p,
                            struct thread_struct *t);

static void l4x_dispatch_suspend(struct task_struct *p,
                                 struct thread_struct *t);
#endif

/* entry/common.c */
#ifdef CONFIG_X86_64
void do_syscall_64(unsigned long nr, struct pt_regs *regs);
#endif

static inline void dispatch_system_call(struct task_struct *p,
                                        struct pt_regs *regsp)
{
	unsigned int syscall;
	int show_syscalls = 0;

#ifdef CONFIG_L4_VCPU
	local_irq_enable();
#endif

	regsp->orig_ax = syscall = regsp->ax;
	regsp->ax = -ENOSYS;

	if (show_syscalls)
		printk("Syscall %3d for %s(%d at %p): args: %lx,%lx,%lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->S_ARG0, regsp->S_ARG1, regsp->S_ARG2);

	if (show_syscalls && syscall == 11) {
		struct filename *fn;
		printk("execve: pid: %d(%s): ", p->pid, p->comm);
		fn = getname((char *)regsp->S_ARG0);
		printk("%s\n", IS_ERR(fn) ? "UNKNOWN" : fn->name);
		putname(fn);
	}

	if (show_syscalls && syscall == 120)
		printk("Syscall %3d for %s(%d at %p): arg1 = %lx ebp=%lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->S_ARG0, regsp->S_ARG5);

	if (show_syscalls && syscall == 21)
		printk("Syscall %3d mount for %s(%d at %p): %lx %lx %lx %lx %lx %lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->S_ARG0, regsp->S_ARG1, regsp->S_ARG2,
		       regsp->S_ARG3, regsp->S_ARG4, regsp->S_ARG5);
	if (show_syscalls && syscall == 5) {
		struct filename *fn = getname((char *)regsp->S_ARG0);
		printk("open: pid: %d(%s): %s (%lx)\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "UNKNOWN" : fn->name, regsp->S_ARG0);
		if (!IS_ERR(fn))
			putname(fn);
	}

	if (unlikely(!is_lx_syscall(syscall))) {
		printk("Syscall %3d for %s(%d at %p): arg1 = %lx\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->S_ARG0);
		l4x_print_regs(p->thread.error_code, p->thread.trap_nr, regsp);
	}


#ifdef CONFIG_X86_64
	do_syscall_64(syscall, regsp);
#else
	do_syscall_32_irqs_on(regsp);
#endif

	if (show_syscalls)
		printk("Syscall %3d for %s(%d at %p): return %lx/%ld\n",
		       syscall, p->comm, p->pid, (void *)regsp->ip,
		       regsp->ax, regsp->ax);
#ifdef CONFIG_L4_VCPU
	local_irq_disable();
#endif
}

/* Rename to l4x_pre_iret_to_user_work */
static unsigned
l4x_pre_iret_to_user_work(struct pt_regs *regs, struct task_struct *p,
                          unsigned long scno, void *dummy,
                          unsigned kernel_context)
{
	local_irq_disable();
	prepare_exit_to_usermode(regs);
	return 0;
}

static int l4x_port_emu_rtc_dummy(bool in, u32 port,
                                  u8 accesslen, u32 *value)
{
	static bool rtc_emu_info;
	if (port != 0x70 && port != 0x71)
		return 0;

	if (in) {
		if (!rtc_emu_info)
			pr_warn("l4x: Faking dummy RTC\n");
		rtc_emu_info = true;
		/* returning 0 will provide
		 * RTC time:  0:00:00, date: 00/00/00
		 * which is better than ~0
		 */
		*value = 0;
	}

	return 1;
}

static int l4x_port_emu_vga_dummy(bool in, u32 port,
                                  u8 accesslen, u32 *value)
{
	if (port < 0x3b0 || port > 0x3df)
		return 0;

	if (in)
		*value = ~0;

	return 1;
}

static int l4x_port_emu_pci_dummy(bool in, u32 port,
                                  u8 accesslen, u32 *value)
{
	if (port != 0xcf8)
		return 0;

	*value = ~0;
	return 1;
}

static int l4x_port_emu_i8042_dummy(bool in, u32 port,
                                    u8 accesslen, u32 *value)
{
	if (port != 0x60 && port != 0x64)
		return 0;

	if (in)
		*value = ~0;

	return 1;
}

/* This allows to add the 8250 serial driver to L4Linux and munges
 * away the exceptions that happen because a particular UART is
 * not available.
 */
static int l4x_port_emu_8250_dummy(bool in, u32 port, u8 accesslen, u32 *value)
{
	if (port == 0x3f9 || port == 0x2f9 || port == 0x3e9 || port == 0x2e9) {
		if (in)
			*value = ~0;
		return 1;
	}

	return 0;
}


typedef int (*l4x_port_emu_function)(bool in, u32 port,
                                     u8 accesslen, u32 *value);

static l4x_port_emu_function l4x_port_emu_funcs[] = {
	l4x_port_emu_rtc_dummy,
	l4x_port_emu_vga_dummy,
	l4x_port_emu_pci_dummy,
	l4x_port_emu_i8042_dummy,
	l4x_port_emu_8250_dummy,
};


/*
 * A primitive emulation.
 *
 * Returns 1 if something could be handled, 0 if not.
 */
static inline int l4x_port_emulation(struct pt_regs *regs)
{
	u8 insn, ilen, accesslen = 0;
	u32 port, value = 0;
	bool in_op = 0;
	int i, ioffset = 0;
	bool operand_size_prefix = 0;
	/* 64bit mode writes zero extends eax to rax */
	const unsigned long operand_mask[] = { 0xff, 0xffff, ~0ul };

	/**
         *   0:   ec                      in     (%dx),%al
         *   1:   66 ed                   in     (%dx),%ax
         *   3:   ed                      in     (%dx),%eax
         *   4:   e4 01                   in     $0x1,%al
         *   6:   66 e5 02                in     $0x2,%ax
         *   9:   e5 03                   in     $0x3,%eax
         *   b:   ee                      out    %al,(%dx)
         *   c:   66 ef                   out    %ax,(%dx)
         *   e:   ef                      out    %eax,(%dx)
         *   f:   e6 01                   out    %al,$0x1
         *  11:   66 e7 02                out    %ax,$0x2
         *  14:   e7 03                   out    %eax,$0x3
	 */

	if (get_user(insn, (char *)regs->ip))
		return 0; /* User memory could not be accessed */

	if (insn == 0x66) {
		operand_size_prefix = 1;
		ioffset = 1;
		if (get_user(insn, (char *)(regs->ip + ioffset)))
			return 0; /* User memory could not be accessed */
	}

	/* no IN or OUT instruction, skip emulation */
	if ((insn & 0xf4) != 0xe4)
		return 0;

	/* handle instructions that support the operand size prefix */
	if (insn & 1) /* handle 0xe5, 0xed, 0xe7, and 0xef */
		/* 16bit if override prefix is on 32bit is default */
		accesslen = operand_size_prefix ? 1 : 2;

	if (insn & 8) {
		/* use DX as IO-port address */
		port = regs->dx & 0xffff;
		ilen = ioffset + 1;
	} else {
		/* use imm8 as port address */
		if (get_user(port, (char *)regs->ip + ioffset + 1))
			return 0; /* User memory could not be accessed */
		ilen = ioffset + 2;
	}

	if (insn & 2) {
		/* OUT operation, load value from eax, ax, or al */
		in_op = 0;
		value = regs->ax & operand_mask[accesslen];
	} else {
		/* IN operation */
		in_op = 1;
	}

	for (i = 0; i < ARRAY_SIZE(l4x_port_emu_funcs); ++i)
		if (l4x_port_emu_funcs[i](in_op, port, accesslen, &value)) {
			regs->ip += ilen;
			if (in_op) {
				/* put the result for IN into al, ax, or eax */
				value &= operand_mask[accesslen];
				regs->ax = (regs->ax & ~operand_mask[accesslen]) | value;
			}
			return 1;
		}

	pr_info("l4x: IO-Port: port=%x %s (insn=0x%x ip=%08lx)\n",
		port, in_op ? "IN" : "OUT", insn, regs->ip);

	return 0; /* Not handled here */
}

static bool l4x_emulate_kdebug;
module_param(l4x_emulate_kdebug, bool, 0);
/*
 * Emulation of (some) jdb commands. The user program may not
 * be allowed to issue jdb commands, they trap in here. Nevertheless
 * hybrid programs may want to use some of them. Emulate them here.
 * Note:  When there's a failure reading the string from user we
 *        nevertheless return true.
 * Note2: More commands to be emulated can be added on request.
 */
static int l4x_kdebug_emulation(struct pt_regs *regs)
{
	u8 op = 0, val;
	char *addr = (char *)regs->ip - 1;
	int i, len;

	if (!l4x_emulate_kdebug)
		return 0;

	if (get_user(op, addr))
		return 0; /* User memory could not be accessed */

	if (op != 0xcc) /* Check for int3 */
		return 0; /* Not for us */

	/* jdb command group */
	if (get_user(op, addr + 1))
		return 0; /* User memory could not be accessed */

	if (op == 0xeb) { /* enter_kdebug */
		if (get_user(len, addr + 2))
			return 0; /* Access failure */
		regs->ip += len + 2;
		outstring("User enter_kdebug text: ");
		for (i = 3; len; len--) {
			if (get_user(val, addr + i++))
				break;
			outchar(val);
		}
		outchar('\n');
		enter_kdebug("User program enter_kdebug");

		return 1; /* handled */

	} else if (op == 0x3c) {
		if (get_user(op, addr + 2))
			return 0; /* Access failure */
		switch (op) {
			case 0: /* outchar */
				outchar(regs->ax & 0xff);
				break;
			case 1: /* outnstring */
				len = regs->bx;
				for (i = 0;
				     !get_user(val, (char *)(regs->ax + i++))
				     && len;
				     len--)
					outchar(val);
				break;
			case 2: /* outstring */
				for (i = 0;
				     !get_user(val, (char *)(regs->ax + i++))
				     && val;)
					outchar(val);
				break;
			case 5: /* outhex32 */
				outhex32(regs->ax);
				break;
			case 6: /* outhex20 */
				outhex20(regs->ax);
				break;
			case 7: /* outhex16 */
				outhex16(regs->ax);
				break;
			case 8: /* outhex12 */
				outhex12(regs->ax);
				break;
			case 9: /* outhex8 */
				outhex8(regs->ax);
				break;
			case 11: /* outdec */
				outdec(regs->ax);
				break;
			default:
				return 0; /* Did not understand */
		};
		regs->ip += 2;
		return 1; /* handled */
	}

	return 0; /* Not handled here */
}

#ifdef CONFIG_L4_VCPU
static inline unsigned long vcpu_val_trapno(l4_vcpu_state_t *v)
{
	return v->r.trapno;
}

static inline unsigned long vcpu_val_err(l4_vcpu_state_t *v)
{
	return v->r.err;
}
#endif

static inline unsigned long thread_val_trapno(struct thread_struct *t)
{
	return t->trap_nr;
}

static inline unsigned long thread_val_err(struct thread_struct *t)
{
	return t->error_code;
}

/*
 * Return values: 0 -> do send a reply
 *                1 -> don't send a reply
 */
static inline int l4x_dispatch_exception(struct task_struct *p,
                                         struct thread_struct *t,
                                         unsigned long err,
                                         unsigned long trapno,
                                         struct pt_regs *regs,
                                         unsigned long pfa)
{
#ifndef CONFIG_L4_VCPU
	l4x_hybrid_do_regular_work();
#endif
	l4x_debug_stats_exceptions_hit();

	if (0) {
#ifndef CONFIG_L4_VCPU
	} else if (trapno == 0xff) {
		/* we come here for suspend events */
		TBUF_LOG_SUSPEND(fiasco_tbuf_log_3val("dsp susp", TBUF_TID(t->l4x.user_thread_id), regs->ip, 0));
		l4x_dispatch_suspend(p, t);

		return 0;
#endif
	} else if (likely(l4x_is_linux_syscall(err, trapno, regs))) {
		TBUF_LOG_INT80(fiasco_tbuf_log_3val("int80  ", TBUF_TID(t->l4x.user_thread_id), regs->ip, regs->ax));

		/* set after int 0x80 / syscall, before doing the system
		 * call so the forked childs get the increase too */
		regs->ip += 2;

		dispatch_system_call(p, regs);

		BUG_ON(p != current);

		return 1;
#ifdef CONFIG_IA32_EMULATION
	} else if (likely(l4x_is_compat_syscall(err, trapno, regs))) {
		/* set after int 0x80, before syscall so the forked childs
		 * get the increase too */
		regs->ip += 2;

		dispatch_compat_system_call(p, regs);

		BUG_ON(p != current);

		return 1;
#endif
	}

	switch (trapno) {
		case X86_TRAP_MF:
			do_coprocessor_error(regs, err);
			break;
		case X86_TRAP_XF:
			do_simd_coprocessor_error(regs, err);
			break;
		case X86_TRAP_NM:
			do_device_not_available(regs, err);
			break;
		case X86_TRAP_DB:
			do_debug(regs, err);
			break;
		case X86_TRAP_BP:
			if (l4x_kdebug_emulation(regs))
				return 0;
			do_int3(regs, err);
			break;
		case X86_TRAP_UD:
			do_invalid_op(regs, err);
			break;
		case X86_TRAP_GP:
#ifndef CONFIG_L4_VCPU
			if (l4x_hybrid_begin(p, t))
				return 0;
#endif
			/* Fall through */
		case X86_TRAP_PF:
			if (l4x_port_emulation(regs))
				return 0;

			do_general_protection(regs, err);
			break;
		default:
			goto unknown_fault;
	}

	return 0;

unknown_fault:
	if (!user_mode(regs)) {
		die("Exception", regs, err);
		return 0;
	}

	TBUF_LOG_EXCP(fiasco_tbuf_log_3val("except ", TBUF_TID(t->l4x.user_thread_id), t->trap_nr, t->error_code));

	if (l4x_deliver_signal(trapno))
		return 0; /* handled signal, reply */

	/* This path should never be reached... */

	printk("(Unknown) EXCEPTION\n");
	l4x_print_regs(err, trapno, regs);

	enter_kdebug("check");

	/* The task somehow misbehaved, so it has to die */
	do_exit(SIGKILL);

	return 0;
}

static inline int l4x_handle_page_fault_with_exception(struct thread_struct *t,
                                                       struct pt_regs *regs)
{
	return 0; // not for us
}

#ifdef CONFIG_L4_VCPU
static inline void l4x_vcpu_entry_user_arch(void)
{
#ifdef CONFIG_X86_32
	asm ("cld          \n"
	     "mov %0, %%gs \n"
	     "mov %1, %%fs \n"
	     : : "r" (l4x_x86_utcb_get_orig_segment()),
#ifdef CONFIG_SMP
	     "r" ((l4x_x86_fiasco_gdt_entry_offset + 2) * 8 + 3)
#else
	     "r" (l4x_x86_utcb_get_orig_segment())
#endif
	     : "memory");
#else
	asm ("cld" : : : "memory");
#endif
}
#endif

#define __INCLUDED_FROM_L4LINUX_DISPATCH
#include "../dispatch.c"
