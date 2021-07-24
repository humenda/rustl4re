
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/processor.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/unistd.h>
#include <asm/pgalloc.h>
#include <asm/exception.h>

#include <l4/sys/cache.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>
#include <l4/util/util.h>
#include <l4/re/consts.h>
#include <l4/log/log.h>


#include <asm/l4lxapi/task.h>
#include <asm/l4lxapi/thread.h>
#include <asm/l4lxapi/memory.h>
#include <asm/l4lxapi/irq.h>
#include <asm/api/macros.h>

#include <asm/generic/dispatch.h>
#include <asm/generic/task.h>
#include <asm/generic/upage.h>
#include <asm/generic/log.h>
#include <asm/generic/memory.h>
#include <asm/generic/setup.h>
#include <asm/generic/ioremap.h>
#include <asm/generic/hybrid.h>
#include <asm/generic/syscall_guard.h>
#include <asm/generic/stats.h>
#include <asm/generic/smp.h>
#include <asm/generic/stack_id.h>

#include <asm/l4x/exception.h>
#include <asm/l4x/fpu.h>
#include <asm/l4x/l4_syscalls.h>
#include <asm/l4x/lx_syscalls.h>
#include <asm/l4x/utcb.h>
#include <asm/l4x/upage.h>
#include <asm/l4x/signal.h>



//#define DEBUG_SYSCALL_PRINTFS

#ifdef DEBUG_SYSCALL_PRINTFS
#include <linux/fs.h>
#endif

#define USER_PATCH_CMPXCHG
#define USER_PATCH_GETTLS
#define USER_PATCH_DMB
enum {
 USER_PATCH_CMPXCHG_SHOW = 0,
 USER_PATCH_GETTLS_SHOW  = 0,
 USER_PATCH_DMB_SHOW     = 0,
};


#if 0
#define TBUF_LOG_IDLE(x)        TBUF_DO_IT(x)
#define TBUF_LOG_WAKEUP_IDLE(x)	TBUF_DO_IT(x)
#define TBUF_LOG_USER_PF(x)     TBUF_DO_IT(x)
#define TBUF_LOG_SWI(x)         TBUF_DO_IT(x)
#define TBUF_LOG_EXCP(x)        TBUF_DO_IT(x)
#define TBUF_LOG_START(x)       TBUF_DO_IT(x)
#define TBUF_LOG_SUSP_PUSH(x)   TBUF_DO_IT(x)
#define TBUF_LOG_DSP_IPC_IN(x)  TBUF_DO_IT(x)
#define TBUF_LOG_DSP_IPC_OUT(x) TBUF_DO_IT(x)
#define TBUF_LOG_SUSPEND(x)     TBUF_DO_IT(x)
#define TBUF_LOG_SWITCH(x)      TBUF_DO_IT(x)
#define TBUF_LOG_HYB_BEGIN(x)   TBUF_DO_IT(x)
#define TBUF_LOG_HYB_RETURN(x)  TBUF_DO_IT(x)

#else

#define TBUF_LOG_IDLE(x)
#define TBUF_LOG_WAKEUP_IDLE(x)
#define TBUF_LOG_USER_PF(x)
#define TBUF_LOG_SWI(x)
#define TBUF_LOG_EXCP(x)
#define TBUF_LOG_START(x)
#define TBUF_LOG_SUSP_PUSH(x)
#define TBUF_LOG_DSP_IPC_IN(x)
#define TBUF_LOG_DSP_IPC_OUT(x)
#define TBUF_LOG_SUSPEND(x)
#define TBUF_LOG_SWITCH(x)
#define TBUF_LOG_HYB_BEGIN(x)
#define TBUF_LOG_HYB_RETURN(x)

#endif

asmlinkage int syscall_trace_enter(struct pt_regs *regs);
asmlinkage int syscall_trace_exit(struct pt_regs *regs);

static DEFINE_PER_CPU(struct l4x_arch_cpu_fpu_state, l4x_cpu_fpu_state);

struct l4x_arch_cpu_fpu_state *l4x_fpu_get(unsigned cpu)
{
	return &per_cpu(l4x_cpu_fpu_state, cpu);
}

static inline int l4x_msgtag_fpu(unsigned cpu)
{
	return L4_MSGTAG_TRANSFER_FPU;
}

static void l4x_fpu_get_info(l4_utcb_t *utcb)
{
	printk("%s %d: do something here\n", __func__, __LINE__); 
//	struct l4x_arch_cpu_fpu_state *x = l4x_fpu_get(smp_processor_id());
//	x->fpexc   = (l4_utcb_mr_u(utcb)->mr[21] & ~0x40000000)
//	             | (x->fpexc & 0x40000000);
//	x->fpinst  = l4_utcb_mr_u(utcb)->mr[22];
//	x->fpinst2 = l4_utcb_mr_u(utcb)->mr[23];
}

static inline int l4x_msgtag_copy_ureg(l4_utcb_t *u)
{
	return 0;
}

enum { L4X_TRIGGERED_EXCEPTION_VAL = 0x3e };

static inline int l4x_is_triggered_exception_from_err(l4_umword_t err)
{
	return (err >> 26) == L4X_TRIGGERED_EXCEPTION_VAL;
}

static inline int l4x_is_triggered_exception_from_val(l4_umword_t val)
{
	return val == L4X_TRIGGERED_EXCEPTION_VAL;
}

static inline unsigned long regs_pc(struct task_struct *p)
{
	return task_pt_regs(p)->pc;
}

static inline unsigned long regs_sp(struct task_struct *p)
{
	return task_pt_regs(p)->sp;
}

static inline void l4x_arch_task_setup(struct thread_struct *t)
{

}

static inline void l4x_arch_do_syscall_trace(struct task_struct *p)
{
	if (unlikely(test_tsk_thread_flag(p, TIF_SYSCALL_TRACE)))
		syscall_trace_exit(task_pt_regs(p));
}

static inline int l4x_hybrid_check_after_syscall(l4_utcb_t *utcb)
{
	printk("%s %d\n", __func__, __LINE__); 
	return 0;
	//l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	//return exc->err == ((0x3d << 26) | 0x40) // after L4 syscall
	//       || (exc->err >> 26) == L4X_TRIGGERED_EXCEPTION_VAL; // L4 syscall exr
}

#ifdef CONFIG_L4_VCPU
static inline void l4x_arch_task_start_setup(l4_vcpu_state_t *v,
		                             struct task_struct *p, l4_cap_idx_t task_cap)
#else
static inline void l4x_arch_task_start_setup(struct task_struct *p, l4_cap_idx_t task_cap)
#endif
{
}

extern void schedule_tail(struct task_struct *prev);

static inline l4_umword_t l4x_l4pfa(struct thread_struct *t)
{
	return t->fault_address;
}

static inline int l4x_iswrpf(unsigned long error_code)
{
	return error_code & (1 << 6);
}

static inline int l4x_ispf(unsigned long error_code)
{
	// ((ec >> 26) & 0x30) == 0x20 && (ec & 0x3f) < 0x10
	return (error_code & 0xc0000030) == 0x80000000;
}

static inline int l4x_ispf_t(struct thread_struct *t)
{
	return l4x_ispf(t->fault_code);
}

static inline int l4x_ispf_v(l4_vcpu_state_t *vcpu)
{
	return l4x_ispf(vcpu->r.err);
}

void l4x_finish_task_switch(struct task_struct *prev);
int  l4x_deliver_signal(int exception_nr);

DEFINE_PER_CPU(struct thread_info *, l4x_current_ti) = &init_task.thread_info;
DEFINE_PER_CPU(struct thread_info *, l4x_current_proc_run);
#ifndef CONFIG_L4_VCPU
static DEFINE_PER_CPU(unsigned, utcb_snd_size);
#endif

void l4x_switch_to(struct task_struct *prev, struct task_struct *next)
{
#ifdef CONFIG_L4_VCPU
	l4_vcpu_state_t *vcpu = l4x_vcpu_ptr[smp_processor_id()];
#endif
	if (0)
		l4x_printf("%s: cpu%d: %s(%d)[%ld] -> %s(%d)[%ld]\n",
		           __func__, smp_processor_id(),
		           prev->comm, prev->pid, prev->state,
		           next->comm, next->pid, next->state);
#ifdef CONFIG_L4_VCPU
	TBUF_LOG_SWITCH(
	     fiasco_tbuf_log_3val("SWITCH", (l4_umword_t)prev->stack,
		     (l4_umword_t)next->stack, 0));
#else
	TBUF_LOG_SWITCH(
	     fiasco_tbuf_log_3val("SWITCH",
				  TBUF_TID(prev->thread.l4x.user_thread_id),
				  TBUF_TID(next->thread.l4x.user_thread_id),
				  0));
#endif /* VCPU */

#ifdef CONFIG_L4_VCPU
#ifdef CONFIG_L4_DEBUG
	if (vcpu->state & L4_VCPU_F_IRQ)
		enter_kdebug("switch + irq");

	{
		register unsigned long sp asm ("sp");
		unsigned long m = ~(THREAD_SIZE - 1);
		if (((l4_umword_t)prev->stack & m) != (sp & m)) {
			printk("prev->stack = %lx  sp = %lx\n",
			       (unsigned long)prev->stack, sp);
			enter_kdebug("prev->stack != sp");
		}
	}
#endif /* DEBUG */

	if (next->mm && prev->mm != next->mm)
		vcpu->user_task = next->mm->context.task;
#endif /* VCPU */

#ifndef CONFIG_L4_VCPU
	per_cpu(l4x_current_ti, smp_processor_id())
	  = (struct thread_info *)next;
#endif /* VCPU */


#if defined(CONFIG_SMP) && !defined(CONFIG_L4_VCPU)
	next->thread.l4x.user_thread_id = next->thread.l4x.user_thread_ids[smp_processor_id()];
	l4x_stack_struct_get((unsigned long)task_stack_page(next))->utcb
		= l4x_stack_struct_get((unsigned long)task_stack_page(prev))->utcb;
#endif

#ifdef CONFIG_L4_VCPU
	mb();
	vcpu->entry_sp = (unsigned long)task_pt_regs(next);

#ifdef CONFIG_L4_DEBUG
	if (next->mm && (vcpu->entry_sp <= (l4_umword_t)next->stack
	    || vcpu->entry_sp > (l4_umword_t)next->stack + THREAD_SIZE)) {
		LOG_printf("stack: %lx  %lx\n", (l4_umword_t)next->stack,
				vcpu->entry_sp);
		enter_kdebug("Wrong entry-sp?");
	}
#endif /* DEBUG */
	mb();
#endif /* VCPU */
}

static inline void l4x_pte_add_access_flag(pte_t *ptep)
{
	pte_val(*ptep) |= PTE_AF;
}

static inline void l4x_pte_add_access_and_dirty_flags(pte_t *ptep)
{
	pte_val(*ptep) |= PTE_AF + PTE_DIRTY;
}

#ifdef CONFIG_L4_VCPU
static inline void
state_to_vcpu(l4_vcpu_state_t *vcpu, struct pt_regs *regs,
              struct task_struct *p)
{
	ptregs_to_vcpu(vcpu, regs);
	vcpu->r.tpidruro = p->thread.uw.tp2_value;
	// Add compat_task check
	asm("msr TPIDR_EL0, %0" : : "r" (p->thread.uw.tp_value));
}

static inline void vcpu_to_thread_struct(l4_vcpu_state_t *v,
                                         struct thread_struct *t)
{
	t->fault_code    = v->r.err;
	t->fault_address = v->r.pfa;
	// Add compat_task check
	asm ("mrs %0, TPIDR_EL0" : "=r" (t->uw.tp_value));
}

static inline void thread_struct_to_vcpu(l4_vcpu_state_t *v,
                                         struct thread_struct *t)
{
}
#else
static inline void utcb_to_thread_struct(l4_utcb_t *utcb,
                                         struct task_struct *p,
                                         struct thread_struct *t)
{
	l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	utcb_exc_to_ptregs(exc, task_pt_regs(p));
	t->fault_code     = exc->err;
	t->fault_address  = exc->pfa;
	t->uw.tp_value    = exc->tpidrurw;
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
	exc->tpidrurw = t->uw.tp_value;
	// TODO: use task-compat to use either tp_value or tp2_value
	exc->tpidruro = t->uw.tp2_value;
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

static inline void l4x_print_regs(unsigned long error_code,
                                  unsigned long trapno, struct pt_regs *r)
{
#define R(nr) r->regs[nr]
	printk(" x0: %016llx  x1: %016llx\n"
	       " x2: %016llx  x3: %016llx\n"
	       " x4: %016llx  x5: %016llx\n"
	       " x6: %016llx  x7: %016llx\n"
	       " x8: %016llx  x9: %016llx\n"
	       "x10: %016llx x11: %016llx\n"
	       "x12: %016llx x13: %016llx\n"
	       "x14: %016llx x15: %016llx\n"
	       "x16: %016llx x17: %016llx\n"
	       "x18: %016llx x19: %016llx\n"
	       "x20: %016llx x21: %016llx\n"
	       "x22: %016llx x23: %016llx\n"
	       "x24: %016llx x25: %016llx\n"
	       "x26: %016llx x27: %016llx\n"
	       "x28: %016llx x29: %016llx\n"
	       "x30: %016llx\n",
	       R(0), R(1), R(2), R(3),
	       R(4), R(5), R(6), R(7),
	       R(8), R(9), R(10), R(11),
	       R(12), R(13), R(14), R(15),
	       R(16), R(17), R(18), R(19),
	       R(20), R(21), R(22), R(23),
	       R(24), R(25), R(26), R(27),
	       R(28), R(29), R(30));
	printk("PC:     %016llx SP:  %016llx\n"
	       "Pstate: %016llx Err: %08lx\n",
	       r->pc, r->sp, r->pstate, error_code);
#undef R
}

static unsigned
l4x_pre_iret_to_user_work(struct pt_regs *regsp, struct task_struct *p,
                          unsigned long syscall, syscall_t *sctbl,
                          unsigned kernel_context)
{
	unsigned long tifl;
	unsigned was_interruptible = 0;
	asmlinkage void do_notify_resume(struct pt_regs *regs,
	                                 unsigned long thread_flags);

	if (kernel_context)
		return 0;

	if (0)
		printk("ret: %s/%d FL=%lx\n",
		       p->comm, p->pid, current_thread_info()->flags);
	goto ret_to_user;

work_pending:
	was_interruptible = 1;

	do_notify_resume(regsp, tifl);
#ifdef CONFIG_TRACE_IRQFLAGS
        trace_hardirqs_on();                 // enabled while in userspace
#endif
	// tifl = current_thread_info()->flags; // re-check for single-step
	goto finish_ret_to_user;

ret_to_user:
	tifl = current_thread_info()->flags;
	if (tifl & _TIF_WORK_MASK)
		goto work_pending;

finish_ret_to_user:
	// enable_step_tsk
	// #ifdef CONFIG_GCC_PLUGIN_STACKLEAK: stackleak_erase
	return was_interruptible;
}

static char *l4x_arm_decode_error_code(unsigned long error_code)
{
	switch (error_code >> 26) {
		case 0x0:
			return "Undefined instruction";
		case 0x7:
			if ((error_code & 0xf) == 0xa)
				return "FPU";
			break;
		case 0x11:
			return "SWI";
		case 0x20:
			if (error_code & 0x40)
				return "Alien syscall return";
			return "Alien syscall";
		case 0x24:
		case 0x25:
			return "Data abort";
		case 0x3f:
			return "IRQ";
	}
	return "Unknown";
}

#ifdef CONFIG_L4_VCPU
static inline unsigned long vcpu_val_trapno(l4_vcpu_state_t *v)
{
	return 0;
}

static inline unsigned long vcpu_val_err(l4_vcpu_state_t *v)
{
	return v->r.err;
}
#endif

static inline unsigned long thread_val_trapno(struct thread_struct *t)
{
	return 0;
}

static inline unsigned long thread_val_err(struct thread_struct *t)
{
	return t->fault_code;
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
	int handled = 0;

#ifndef CONFIG_L4_VCPU
	l4x_hybrid_do_regular_work();
#endif
	l4x_debug_stats_exceptions_hit();

#ifndef CONFIG_L4_VCPU
	if (l4x_is_triggered_exception_from_err(err)) {
		/* we come here for suspend events */
		TBUF_LOG_SUSPEND(fiasco_tbuf_log_3val("dsp susp", TBUF_TID(t->l4x.user_thread_id), regs->pc, 0));

		l4x_kuser_cmpxchg_check_and_fixup(regs);
		l4x_dispatch_suspend(p, t);

		return 0;
	}
#endif

	// adjust pc to point at the insn
	//regs->pc -= 4;

	//fiasco_tbuf_log_3val("EXC", TBUF_TID(t->l4x.user_thread_id), regs->pc, err);

	if ((err >> 26) == 0x15) {
#ifdef CONFIG_L4_VCPU
		local_irq_enable();
#endif
		do_el0_svc(regs);

		l4x_pre_iret_to_user_work(regs, p, 0, 0, 0);

		BUG_ON(p != current);

		return 1;
	} else if (err >> 26 == 0x24) {
		regs->pc += 4;
		do_mem_abort(pfa, err, regs);
		return 0;

	} else if (err >> 26 == 0x3c) {
		do_debug_exception(pfa, err, regs);

#ifndef CONFIG_L4_VCPU
	} else if (err >> 26 == 0x3d) {
		/* Syscall alien exception */
		if (l4x_hybrid_begin(p, t))
			return 0;
#endif
	}

	TBUF_LOG_EXCP(fiasco_tbuf_log_3val("except ", TBUF_TID(t->l4x.user_thread_id), err, regs->pc));

	if ((err >> 26) == 0x07
	    && (   ((err & 0xe) == 10)
	        || (err & (1 << 5)))) {

		asmlinkage void do_fpsimd_acc(unsigned int esr,
		                              struct pt_regs *regs);
		do_fpsimd_acc(err, regs);

		handled = 1;
	}



	// adjust pc to point at the insn again
	//regs->pc -= 4;

	if (likely(handled))
		return 0; /* handled */

	if ((err >> 26) == 0) {
		asmlinkage void do_undefinstr(struct pt_regs *regs);
		do_undefinstr(regs);
		return 0;
	}

	// still not handled, PC is on the insn now

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
	l4x_print_regs(thread_val_err(t), thread_val_trapno(t), regs);

	if ((err >> 26) == 0) {
		unsigned long val;
		int ret;

		ret = get_user(val, (unsigned long *)regs->pc);

		if (ret) {
			printk("get_user error: %d\n", ret);
			val = ~0UL;
		}
		printk(" %s/%d: Undefined instruction at"
		       " %016llx with content %08lx, err %08lx\n",
		       p->comm, p->pid, regs->pc, val, err);
		l4x_print_vm_area_maps(p, regs->pc);
		l4x_print_regs(thread_val_err(&p->thread),
		               thread_val_trapno(&p->thread), regs);
		if (0)
			enter_kdebug("undef insn");
	}
#endif

	if (!user_mode(regs)) {
		printk("DIE HERE...\n");
		//die("Exception", regs, err);
		return 0;
	}

	if (l4x_deliver_signal(0))
		return 0; /* handled signal, reply */

	printk("Error code: %s\n", l4x_arm_decode_error_code(err));
	printk("(Unknown) EXCEPTION\n");
	l4x_print_regs(thread_val_err(t), thread_val_trapno(t), regs);

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
	enter_kdebug("check");
#endif

	/* The task somehow misbehaved, so it has to die */
	do_exit(SIGKILL);

	return 0;
}

static inline void l4x_set_kuser_tls_ver(struct pt_regs *regs,
                                         unsigned long pfa, int targetreg,
                                         unsigned long op, unsigned long pc,
                                         int thumb)
{
#if 0
	if (targetreg == -1) {
		LOG_printf("Lx: Unknown %sopcode %lx at %lx\n",
		           thumb ? "thumb " : "", op, pc);
		return;
	}

	if ((pfa & 0xf) == 0xc)
		regs->regs[targetreg] = *(unsigned long *)(upage_addr + 0xffc);
	else
		regs->regs[targetreg] = current->task.uw.tp_value[0];
#endif
}

static inline int l4x_handle_page_fault_with_exception(struct thread_struct *t,
                                                       struct pt_regs *regs)
{
	return 0; // not for us
}

static inline int l4x_handle_io_page_fault(struct task_struct *p,
                                           l4_umword_t pfa,
                                           l4_umword_t *d0, l4_umword_t *d1)
{
	return 1;
}

#ifdef CONFIG_L4_VCPU
static inline void l4x_vcpu_entry_user_arch(void)
{
}
#endif

#define __INCLUDED_FROM_L4LINUX_DISPATCH
#include "../dispatch.c"
