
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
#include <asm/unistd-oabi.h>
#include <asm/pgalloc.h>
#include <asm/tls.h>
#include <asm/opcodes.h>

#include <l4/sys/cache.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>
#include <l4/util/util.h>
#include <l4/log/log.h>
#include <l4/re/consts.h>


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

asmlinkage int syscall_trace_enter(struct pt_regs *regs, int scno);
asmlinkage int syscall_trace_exit(struct pt_regs *regs, int scno);

static DEFINE_PER_CPU(struct l4x_arch_cpu_fpu_state, l4x_cpu_fpu_state);

struct l4x_arch_cpu_fpu_state *l4x_fpu_get(unsigned cpu)
{
	return &per_cpu(l4x_cpu_fpu_state, cpu);
}

static inline int l4x_msgtag_fpu(unsigned cpu)
{
	return (l4x_fpu_get(cpu)->fpexc & (1 << 30)) ?  L4_MSGTAG_TRANSFER_FPU : 0;
}

#ifdef CONFIG_VFP
static void l4x_fpu_get_info(l4_utcb_t *utcb)
{
	struct l4x_arch_cpu_fpu_state *x = l4x_fpu_get(smp_processor_id());
	x->fpexc   = (l4_utcb_mr_u(utcb)->mr[21] & ~0x40000000)
	             | (x->fpexc & 0x40000000);
	x->fpinst  = l4_utcb_mr_u(utcb)->mr[22];
	x->fpinst2 = l4_utcb_mr_u(utcb)->mr[23];
}
#endif

static inline int l4x_msgtag_copy_ureg(l4_utcb_t *u)
{
	if (tls_emu || has_tls_reg) {
		l4_utcb_mr_u(u)->mr[25] = current_thread_info()->tp_value[0];
		return 0x8000;
	}
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
	return task_pt_regs(p)->ARM_pc;
}

static inline unsigned long regs_sp(struct task_struct *p)
{
	return task_pt_regs(p)->ARM_sp;
}

static inline void l4x_arch_task_setup(struct thread_struct *t)
{

}

static inline void l4x_arch_do_syscall_trace(struct task_struct *p)
{
	if (unlikely(test_tsk_thread_flag(p, TIF_SYSCALL_TRACE)))
		syscall_trace_exit(task_pt_regs(p), __NR_fork);
}

static inline int l4x_hybrid_check_after_syscall(l4_utcb_t *utcb)
{
	l4_exc_regs_t *exc = l4_utcb_exc_u(utcb);
	return exc->err == ((0x3d << 26) | 0x40) // after L4 syscall
	       || (exc->err >> 26) == L4X_TRIGGERED_EXCEPTION_VAL; // L4 syscall exr
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
	return t->address;
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
	return l4x_ispf(t->error_code);
}

static inline int l4x_ispf_v(l4_vcpu_state_t *vcpu)
{
	return l4x_ispf(vcpu->r.err);
}

void l4x_finish_task_switch(struct task_struct *prev);
int  l4x_deliver_signal(int exception_nr);

DEFINE_PER_CPU(struct thread_info *, l4x_current_ti) = &init_thread_info;
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
		l4x_printf("%s: cpu%d: %s(%d)[%ld] -> %s(%d)[%ld]\n     :: %p -> %p\n",
		           __func__, smp_processor_id(),
		           prev->comm, prev->pid, prev->state,
		           next->comm, next->pid, next->state,
		           prev->stack, next->stack);
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
		if (((l4_umword_t)prev->stack & 0xffffe000) != (sp & 0xffffe000)) {
			enter_kdebug("prev->stack != sp");
		}
	}
#endif /* DEBUG */

	if (next->mm && prev->mm != next->mm)
		vcpu->user_task = next->mm->context.task;
#endif /* VCPU */

#ifndef CONFIG_L4_VCPU
	per_cpu(l4x_current_ti, smp_processor_id())
	  = (struct thread_info *)((unsigned long)next->stack & ~(THREAD_SIZE - 1));
#endif /* VCPU */

#ifdef CONFIG_L4_ARM_UPAGE_TLS
	*(unsigned long *)(upage_addr + UPAGE_USER_TLS_OFFSET) = task_thread_info(next)->tp_value[0];
#endif

#if defined(CONFIG_SMP) && !defined(CONFIG_L4_VCPU)
	next->thread.l4x.user_thread_id = next->thread.l4x.user_thread_ids[smp_processor_id()];
	l4x_stack_struct_get(next->stack)->utcb
		= l4x_stack_struct_get(prev->stack)->utcb;
#endif

#ifdef CONFIG_L4_VCPU
	mb();
	vcpu->entry_sp = (unsigned long)task_pt_regs(next);

#ifdef CONFIG_L4_DEBUG
	if (next->mm && (vcpu->entry_sp <= (l4_umword_t)next->stack
	    || vcpu->entry_sp > (l4_umword_t)next->stack + 0x2000)) {
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
	pte_val(*ptep) |= L_PTE_YOUNG;
}

static inline void l4x_pte_add_access_and_dirty_flags(pte_t *ptep)
{
	pte_val(*ptep) |= L_PTE_YOUNG + L_PTE_DIRTY;
}

#ifdef CONFIG_L4_VCPU
static inline void
state_to_vcpu(l4_vcpu_state_t *vcpu, struct pt_regs *regs,
              struct task_struct *p)
{
	ptregs_to_vcpu(vcpu, regs);
	vcpu->r.tpidruro = task_thread_info(p)->tp_value[0];
}

static inline void vcpu_to_thread_struct(l4_vcpu_state_t *v,
                                         struct thread_struct *t)
{
	t->error_code = v->r.err;
	t->address    = v->r.pfa;
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
	t->error_code     = exc->err;
	t->address        = exc->pfa;
}
#endif

static inline void thread_struct_to_utcb(struct task_struct *p,
                                         struct thread_struct *t,
                                         struct pt_regs *regs,
                                         l4_utcb_t *utcb,
                                         unsigned int send_size)
{
	ptregs_to_utcb_exc(regs, l4_utcb_exc_u(utcb));
	l4_utcb_exc_u(utcb)->tpidruro = task_thread_info(current)->tp_value[0];
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
#define R(nr) r->uregs[nr]
	printk("0: %08lx %08lx %08lx %08lx  4: %08lx %08lx %08lx %08lx\n",
	       R(0), R(1), R(2), R(3), R(4), R(5), R(6), R(7));
	printk("8: %08lx %08lx %08lx %08lx 12: %08lx [01;34m%08lx[0m "
	       "%08lx [01;34m%08lx[0m\n",
	       R(8), R(9), R(10), R(11), R(12), R(13), R(14), R(15));
	printk("CPSR: %08lx Err: %08lx\n", r->ARM_cpsr, error_code);
#undef R
}

static inline void call_system_call_args(syscall_t *sctbl,
                                         unsigned long syscall,
                                         unsigned long arg1,
                                         unsigned long arg2,
                                         unsigned long arg3,
                                         unsigned long arg4,
                                         unsigned long arg5,
                                         unsigned long arg6,
                                         struct pt_regs *regsp)
{
	syscall_t syscall_fn;

#ifdef DEBUG_SYSCALL_PRINTFS
	int show_syscalls = 0;

	if (show_syscalls)
		printk("Syscall call: %ld for %d(%s, pc=%p, lr=%p, sp=%08lx, "
		       "cpu=%d) (%08lx %08lx %08lx %08lx %08lx %08lx)\n",
		       syscall, current->pid, current->comm,
		       (void *)regsp->ARM_pc,
		       (void *)regsp->ARM_lr, regsp->ARM_sp,
		       raw_smp_processor_id(),
		       arg1, arg2, arg3, arg4, arg5, arg6);

	if (0 && show_syscalls && syscall == 11) {
		struct filename *fn = getname((char *)arg1);
		printk("execve: pid: %d(%s) %d: %s (%08lx)\n",
		       current->pid, current->comm, current_uid().val,
		       IS_ERR(fn) ? "INVALID" : fn->name, arg1);
		putname(fn);
	}
	if (0 && show_syscalls && (syscall == 1 || syscall == 248))
		printk("exit: pid: %d(%s)\n",
		       current->pid, current->comm);
	if (0 && syscall == 26)
		printk("ptrace: pid: %d(%s) req=%ld pid=%ld\n",
		       current->pid, current->comm, arg1, arg2);
	if (0 && show_syscalls && syscall == 2) {
		printk("fork: pid: %d(%s)\n",
		       current->pid, current->comm);
	}
	if (0 && show_syscalls && syscall == 3) {
		printk("read: pid: %d(%s): fd = %ld len = %ld\n",
		       current->pid, current->comm, arg1, arg3);
	}
	if (0 && show_syscalls && syscall == 4) {
		printk("write: pid: %d(%s): fd = %ld len = %ld\n",
		       current->pid, current->comm, arg1, arg3);
	}
	if (0 && show_syscalls && syscall == 5) {
		struct filename *fn = getname((char *)arg1);
		printk("open: pid: %d(%s): %s (%lx)\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "INVALID" : fn->name, arg1);
		putname(fn);
	}
	if (0 && show_syscalls && syscall == 12) {
		struct filename *fn = getname((char *)arg1);
		printk("chdir: pid: %d(%s): %s\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "INVALID" : fn->name);
		putname(fn);
	}
	if (0 && show_syscalls && syscall == 14) {
		struct filename *fn = getname((char *)arg1);
		printk("mknod: pid: %d(%s): %s dev=%lx\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "INVALID" : fn->name, arg3);
		putname(fn);
	}
	if (0 && show_syscalls && syscall == 39) {
		struct filename *fn = getname((char *)arg1);
		printk("mkdir: pid: %d(%s): %s (%lx)\n",
		       current->pid, current->comm,
		       IS_ERR(fn) ? "INVALID" : fn->name, arg1);
		putname(fn);
	}
	if (0 && show_syscalls && syscall == 21) {
		struct filename *f1 = getname((char *)arg1);
		struct filename *f2 = getname((char *)arg2);
		printk("mount: pid: %d(%s): %s -> %s\n",
		       current->pid, current->comm,
		       IS_ERR(f1) ? "INVALID" : f1->name,
		       IS_ERR(f2) ? "INVALID" : f2->name);
		putname(f1);
		putname(f2);
	}
	if (0 && show_syscalls && syscall == 45)
		printk("brk: pid: %d(%s): %lx\n", current->pid, current->comm, arg1);
	if (0 && show_syscalls && syscall == 54)
		printk("ioctl: pid: %d(%s): %lx\n",
		       current->pid, current->comm, arg1);
	if (0 && show_syscalls && syscall == 114)
		printk("wait4: pid: %d(%s): (%lx)\n", current->pid, current->comm, arg1);
	if (0 && show_syscalls && syscall == 119)
		printk("sigreturn: pid: %d(%s)\n", current->pid, current->comm);
	if (0 && show_syscalls && syscall == 120)
		printk("clone: pid: %d(%s)\n", current->pid, current->comm);
	if (0 && show_syscalls && syscall == 168)
		printk("poll: pid: %d(%s)\n", current->pid, current->comm);
	if (0 && show_syscalls && syscall == 190)
		printk("vfork: pid: %d(%s)\n", current->pid, current->comm);
	if (0 && show_syscalls && syscall == 192)
		printk("mmap2 size: pid: %d(%s): %lx\n",
		       current->pid, current->comm, arg2);
	if (0 && show_syscalls && syscall == 195) {
		struct filename *path = getname((char *)arg1);
		printk("stat64: pid: %d(%s): %s (%lx)\n",
		       current->pid, current->comm,
		       IS_ERR(path) ? "INVALID" : path->name, arg1);
		putname(path);
	}
	if (0 && show_syscalls && syscall == 217)
		printk("getdents64: pid: %d(%s): (%ld, %lx, %ld)\n",
		       current->pid, current->comm,
		       arg1, arg2, arg3);
	if (0 && show_syscalls && syscall == 221) {
		printk("fcntl64: pid: %d(%s): (%lx, %lx, %lx)\n",
		       current->pid, current->comm,
		       arg1, arg2, arg3);
	}
	if (0 && show_syscalls && syscall == 266)
		printk("statfs: pid: %d(%s): (%lx, %lx, %lx) sp=%lx\n",
		       current->pid, current->comm,
		       arg1, arg2, arg3, regsp->ARM_sp);
#endif

	/* ============================================================ */

	if (likely(is_lx_syscall(syscall))
	           && ((syscall_fn = sctbl[syscall]))) {

		if (likely(!(current_thread_info()->flags & _TIF_SYSCALL_WORK))) {
			regsp->ARM_r0 = syscall_fn(arg1, arg2, arg3, arg4, arg5, arg6);
		} else {
			syscall_trace_enter(regsp, syscall);
			regsp->ARM_r0 = syscall_fn(arg1, arg2, arg3, arg4, arg5, arg6);
			syscall_trace_exit(regsp, syscall);
		}
	} else
		regsp->ARM_r0 = -ENOSYS;

	/* ============================================================ */
#ifdef DEBUG_SYSCALL_PRINTFS
	if (0 && show_syscalls && syscall == 120) {
		printk("syscall-120 result: pid: %d(%s): %lx\n",
		       current->pid, current->comm,
		       regsp->ARM_r0);
	}
	if (show_syscalls)
		printk("Syscall %ld return: 0x%lx\n", syscall, regsp->ARM_r0);
#endif
}

static inline void dispatch_system_call(syscall_t *sctbl,
                                        struct task_struct *p,
                                        unsigned long syscall,
                                        struct pt_regs *regsp)
{
#ifdef CONFIG_L4_VCPU
	local_irq_enable();
#endif

	regsp->ARM_ORIG_r0 = regsp->ARM_r0;

	if (unlikely(syscall == __NR_syscall - __NR_SYSCALL_BASE)) {
		call_system_call_args(sctbl, regsp->ARM_r0,
		                      regsp->ARM_r1, regsp->ARM_r2,
		                      regsp->ARM_r3, regsp->ARM_r4,
		                      regsp->ARM_r5, regsp->ARM_r6,
		                      regsp);
	} else {
		call_system_call_args(sctbl, syscall,
		                      regsp->ARM_r0, regsp->ARM_r1,
		                      regsp->ARM_r2, regsp->ARM_r3,
		                      regsp->ARM_r4, regsp->ARM_r5,
		                      regsp);
	}
}

static unsigned
l4x_pre_iret_to_user_work(struct pt_regs *regsp, struct task_struct *p,
                          unsigned long syscall, syscall_t *sctbl,
                          unsigned kernel_context)
{
	unsigned long tifl;
	unsigned was_interruptible = 0;

	if (kernel_context)
		return 0;

check_work_pending:
	tifl = current_thread_info()->flags;
	if (tifl & _TIF_WORK_MASK)
		goto work_pending;

	goto no_work_pending;

work_pending:
	was_interruptible = 1;
	if (!do_work_pending(regsp, tifl, syscall))
		goto no_work_pending;

	goto local_restart;

no_work_pending:
	return was_interruptible;

local_restart:
	dispatch_system_call(sctbl, p,
	                     __NR_restart_syscall - __NR_SYSCALL_BASE, regsp);

	goto check_work_pending;
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

/* XXX: Move that out to a separate file to avoid the VM_EXEC clash
 *      (they have the same value, but anyway... */
#undef VM_EXEC
#include <asm/asm-offsets.h>
#include <linux/stringify.h>

/* FP emu */
asm(
"	.data						\n"
"	.global fp_enter				\n"
"fp_enter:						\n"
"	.word callswi					\n"
"	.text						\n"
"callswi:						\n"
"	swi #2						\n"
);

/*
 * We directly call into the nwfpe code and do not take the fp_enter hook,
 * because otherwise the sp handling would be a bit too tricky.
 */
#ifndef CONFIG_FPE_NWFPE
static inline unsigned int EmulateAll(unsigned int opcode)
{
	return 0;
}
#else
unsigned int EmulateAll(unsigned int opcode);

struct pt_regs *l4x_fp_get_user_regs(void)
{
	return task_pt_regs(current);
}
#endif

static inline int sc_get_user_4(unsigned long *store, unsigned long addr)
{
	if (l4x_is_upage_user_addr(addr)) {
		*store = *(unsigned long *)
			   (upage_addr + (addr - UPAGE_USER_ADDRESS));
		return 0;
	}
	return get_user(*store, (unsigned long *)addr);
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
	int handled = 0;

#ifndef CONFIG_L4_VCPU
	l4x_hybrid_do_regular_work();
#endif
	l4x_debug_stats_exceptions_hit();

#ifndef CONFIG_L4_VCPU
	if (l4x_is_triggered_exception_from_err(err)) {
		/* we come here for suspend events */
		TBUF_LOG_SUSPEND(fiasco_tbuf_log_3val("dsp susp", TBUF_TID(t->l4x.user_thread_id), regs->ARM_pc, 0));

		l4x_kuser_cmpxchg_check_and_fixup(regs);
		l4x_dispatch_suspend(p, t);

		return 0;
	}
#endif

	// adjust pc to point at the insn
	regs->ARM_pc -= thumb_mode(regs) ? 2 : 4;

	//fiasco_tbuf_log_3val("EXC", TBUF_TID(t->l4x.user_thread_id), regs->ARM_pc, err);

	if ((err >> 26) == 0x11) {
#if defined(CONFIG_OABI_COMPAT) || defined(CONFIG_ARM_THUMB) || !defined(CONFIG_AEABI)
		unsigned long ret = 0;
#endif
#if defined(CONFIG_OABI_COMPAT)
		unsigned long insn;
#endif
		unsigned long scno = regs->uregs[7];
		syscall_t *tbl;

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
		if (0 && unlikely(l4x_is_upage_user_addr(regs->ARM_pc))) {
			printk("Got SWI at %08lx\n", regs->ARM_pc);
			l4x_print_vm_area_maps(p, regs->ARM_pc);
			l4x_print_regs(thread_val_err(&p->thread),
			               thread_val_trapno(&p->thread), regs);
			goto go_away;
		}
#endif

#if defined(CONFIG_OABI_COMPAT)
		/*
		 * If we have CONFIG_OABI_COMPAT then we need to look at the swi
		 * value to determine if it is an EABI or an old ABI call.
		 */
		if (thumb_mode(regs))
			insn = 0;
		else
			ret = sc_get_user_4(&insn, regs->ARM_pc);

#elif defined(CONFIG_AEABI)
		/* Pure EABI user space always put syscall number into scno (r7) */
#elif defined(CONFIG_ARM_THUMB)
		/* Legacy ABI only, possibly thumb mode. */
		if (thumb_mode(regs))
			scno &= __NR_SYSCALL_BASE;
		else
			ret = sc_get_user_4(&scno, regs->ARM_pc);
#else
		ret = sc_get_user_4(&scno, regs->ARM_pc);
#endif


		//printk("1: insn: %lx   scno: %lx  ret: %lx pc=%lx\n", insn, scno, ret, regs->ARM_pc);

		tbl = sys_call_table;

#if defined(CONFIG_OABI_COMPAT)
		/*
		 * If the swi argument is zero, this is an EABI call and we do nothing.
		 *
		 * If this is an old ABI call, get the syscall number into scno and
		 * get the old ABI syscall table address.
		 */
		insn &= ~0xff000000;
		if (insn) {
			scno = insn ^ __NR_OABI_SYSCALL_BASE;
			tbl = sys_oabi_call_table;
		}
#elif !defined(CONFIG_AEABI)
		scno &= ~0xff000000;
		scno ^= __NR_SYSCALL_BASE;
#endif


		TBUF_LOG_SWI(fiasco_tbuf_log_3val("swi    ", TBUF_TID(t->l4x.user_thread_id), regs->ARM_pc, scno));

		regs->ARM_pc += thumb_mode(regs) ? 2 : 4;

		// handle private ARM syscalls?
		if (unlikely((__ARM_NR_BASE - __NR_SYSCALL_BASE) < scno
		             && scno <= (__ARM_NR_BASE - __NR_SYSCALL_BASE) + 5)) {
			// from traps.c
			asmlinkage int arm_syscall(int no, struct pt_regs *regs);
			regs->ARM_r0 = arm_syscall(scno | __ARM_NR_BASE, regs);
			return 0;
		}

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
		if (unlikely(!is_lx_syscall(scno))) {
			printk("Hmm, rather unknown syscall nr 0x%lx/%ld\n", scno, scno);
			l4x_print_vm_area_maps(p, ~0UL);
			l4x_print_regs(thread_val_err(&p->thread),
			               thread_val_trapno(&p->thread), regs);
		}
#endif

		dispatch_system_call(tbl, p, scno, regs);

		l4x_pre_iret_to_user_work(regs, p, scno, tbl, 0);

		BUG_ON(p != current);

		return 1;
	} else if (err >> 26 == 0x24) {
		unsigned long fsr = err & 0x7f; /* FSR_FS5_0 + FSR_WRITE */
		regs->ARM_pc += thumb_mode(regs) ? 2 : 4;
		do_DataAbort(pfa, fsr, regs);
		return 0;

#ifndef CONFIG_L4_VCPU
	} else if (err >> 26 == 0x3d) {
		/* Syscall alien exception */
		if (l4x_hybrid_begin(p, t))
			return 0;
#endif
	}

	TBUF_LOG_EXCP(fiasco_tbuf_log_3val("except ", TBUF_TID(t->l4x.user_thread_id), err, regs->ARM_pc));

#ifdef CONFIG_VFP
	if ((err >> 26) == 0x07
	    && (   ((err & 0xe) == 10)
	        || (err & (1 << 5)))) {
		unsigned long insn;
		int ret;
		l4x_fpu_get_info(l4_utcb());

		if (thumb_mode(regs)) {
			insn = 0;
			ret = get_user(insn, (unsigned short *)regs->ARM_pc);
		} else
			ret = get_user(insn, (unsigned long *)regs->ARM_pc);

		if (!ret) {
			register unsigned long _ret   asm ("r0");
			register unsigned long _insn  asm ("r7")  = insn;
			register unsigned long _pc    asm ("r9")  = regs->ARM_pc + 4;
			register unsigned long _regsp asm ("r8")  = (unsigned long)task_pt_regs(p);
			register unsigned long _ti    asm ("r10") = (unsigned long)current_thread_info();
			asm volatile(
#ifdef CONFIG_FRAME_POINTER
			             "push    {fp}                   \t\n"
#endif
				     "sub     sp, sp, %[PTREGS_SIZE] \t\n"
				     "mov     r0, sp                 \t\n"
				     "mov     r1, %[ptregs]          \t\n"
				     "mov     r2, %[PTREGS_SIZE]     \t\n"
				     "bl      memcpy                 \t\n"
				     "mov     r0, %[insn]            \t\n"
				     "mov     r2, %[afterpc]         \t\n"
			             "adr     r9, 1f                 \t\n"
			             "bl      do_vfp                 \t\n"
				     "mov     r0, #0                 \t\n"
				     "b       2f                     \t\n"
			             "1: @success                    \t\n"
				     "        mov r0, #1             \t\n"
				     "2:                             \t\n"
				     "add     sp, sp, %[PTREGS_SIZE] \t\n"
#ifdef CONFIG_FRAME_POINTER
				     "pop {fp}                       \t\n"
#endif
			             :               "=r" (_ret),
				                     "=r" (_insn)
			             : [insn]        "r" (_insn),
			               [afterpc]     "r" (_pc),
			                             "r" (_ti),
				       [ptregs]      "r" (_regsp),
				       [PTREGS_SIZE] "I" (sizeof(*regs))
			             : "memory", "cc",
				       "r1", "r2", "r3", "r4", "r5", "r6",
				       "r12", "lr"
#ifndef CONFIG_FRAME_POINTER
				     , "r11"
#endif
				     );
			if (_ret)
				return 0; // success with vfp stuff
			// possibility to do it better: loop as below
		}
	}
#endif

	while (1) {
		unsigned long insn;
		int ret;

		if (thumb_mode(regs)) {
			insn = 0;
			ret = get_user(insn, (unsigned short *)(regs->ARM_pc));
			regs->ARM_pc += 2;
		} else {
			ret = get_user(insn, (unsigned long *)(regs->ARM_pc));
			regs->ARM_pc += 4;
		}
		//LOG_printf("insn: %lx\n", insn);

		if (ret)
			break;

		if (arm_check_condition(insn, regs->ARM_cpsr) == ARM_OPCODE_CONDTEST_FAIL)
			break;

		if (EmulateAll(insn))
			handled = 1;
		else
			break;
	}

	// adjust pc to point at the insn again
	regs->ARM_pc -= thumb_mode(regs) ? 2 : 4;

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

		if (thumb_mode(regs)) {
			val = 0;
			ret = get_user(val, (unsigned short *)regs->ARM_pc);
		} else
			ret = get_user(val, (unsigned long *)regs->ARM_pc);

		if (ret) {
			printk("get_user error: %d\n", ret);
			val = ~0UL;
		}
		printk(" %s/%d: Undefined instruction at"
		       " %08lx with content %08lx, err %08lx\n",
		       p->comm, p->pid, regs->ARM_pc, val, err);
		l4x_print_vm_area_maps(p, regs->ARM_pc);
		l4x_print_regs(thread_val_err(&p->thread),
		               thread_val_trapno(&p->thread), regs);
		if (0)
			enter_kdebug("undef insn");
	}
#endif

	if (!user_mode(regs)) {
		die("Exception", regs, err);
		return 0;
	}

	if (l4x_deliver_signal(0))
		return 0; /* handled signal, reply */

#ifdef CONFIG_L4_DEBUG_SEGFAULTS
go_away:
#endif

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
	if (targetreg == -1) {
		LOG_printf("Lx: Unknown %sopcode %lx at %lx\n",
		           thumb ? "thumb " : "", op, pc);
		return;
	}

	if ((pfa & 0xf) == 0xc)
		regs->uregs[targetreg] = *(unsigned long *)(upage_addr + 0xffc);
	else
		regs->uregs[targetreg] = current_thread_info()->tp_value[0];
}

static inline int l4x_handle_page_fault_with_exception(struct thread_struct *t,
                                                       struct pt_regs *regs)
{
	if (0 && (regs->ARM_pc >= TASK_SIZE || l4x_l4pfa(t) >= TASK_SIZE))
		printk("PC/PF>TS: PC=%08lx PF=%08lx LR=%08lx\n",
		       regs->ARM_pc, l4x_l4pfa(t), regs->ARM_lr);

	l4x_kuser_cmpxchg_check_and_fixup(regs);

	// software TLS value and __kuser_helper_version
	if (l4x_l4pfa(t) == 0xffff0ff0 || l4x_l4pfa(t) == 0xffff0ffc) {
		unsigned long pc = regs->ARM_pc;
		int targetreg = -1;

		if (thumb_mode(regs)) {
			unsigned short op;
			get_user(op, (unsigned short *)pc);
			if ((op & 0xf800) == 0x6800) // ldr
				targetreg = op & 7;

			l4x_set_kuser_tls_ver(regs, l4x_l4pfa(t), targetreg,
			                      op, pc, 1);
			regs->ARM_pc += 2;
		} else {
			unsigned long op;
			get_user(op, (unsigned long *)pc);
			if ((op & 0x0e100000) == 0x04100000)
				targetreg = (op >> 12) & 0xf;

			l4x_set_kuser_tls_ver(regs, l4x_l4pfa(t), targetreg,
			                      op, pc, 0);
			regs->ARM_pc += 4;
		}
		return 1; // handled
	}

	// __kuser_get_tls
	if (l4x_l4pfa(t) == 0xffff0fe0 && regs->ARM_pc == 0xffff0fe0) {
#ifdef USER_PATCH_GETTLS
		unsigned long val;

		if (USER_PATCH_GETTLS_SHOW) {
			unsigned long op;
			printk("GETTLS hit at lr=%lx pc=%lx\n", regs->ARM_lr, regs->ARM_pc);
			get_user(op, (unsigned long *)regs->ARM_lr);
			fiasco_tbuf_log_3val("TLSfnc", regs->ARM_lr, regs->ARM_pc, op);
		}

		if (regs->ARM_lr & 1)
			goto trap_and_emulate;

		if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 4))))
			goto trap_and_emulate;

		if ((val & 0xff000000) == 0xeb000000) {
			/* Code:
			   000080f0 <__aeabi_read_tp>:
			   __aeabi_read_tp():
			    80f0:       e3e00a0f        mvn     r0, #61440      ; 0xf000
			    80f4:       e240f01f        sub     pc, r0, #31
			    80f8:       e1a00000        nop                     ; (mov r0, r0)
			    80fc:       e1a00000        nop                     ; (mov r0, r0)

			   ....

			   xxxxx:       ebyyyyyy        bl      80f0 <__aeabi_read_tp>
			 */
			unsigned long tlsfunc, offset, flags;

			val &= 0x00ffffff;
			if (val & (1 << 23))
				val = regs->ARM_lr + 4 - (0x1000000 - val) * 4;
			else
				val = regs->ARM_lr + 4 + val * 4;

			tlsfunc = val;

			val = parse_ptabs_read(val, &offset, &flags);
			if (val == -EFAULT)
				goto trap_and_emulate;
			val += offset;

			if (*(unsigned long *)val != 0xe3e00a0f) {
				local_irq_restore(flags);
				goto trap_and_emulate;
			}

			if (unlikely((val & L4_PAGEMASK) != ((val + 4) & L4_PAGEMASK))) {
				local_irq_restore(flags);
				goto trap_and_emulate;
			}

			if (*(unsigned long *)(val + 4) != 0xe240f01f) {
				local_irq_restore(flags);
				goto trap_and_emulate;
			}

			*(unsigned long *)(val +  0) = 0xe3a00103; // mov r0, #0xc0000000
			*(unsigned long *)(val +  4) = 0xe240f020; // sub pc, r0, #32

			l4_cache_coherent(val, val + 12);

			local_irq_restore(flags);

			regs->ARM_pc = tlsfunc;
			if (USER_PATCH_GETTLS_SHOW)
				printk("  handled (1) -> %lx\n", regs->ARM_pc);
			return 1; // handled
		}

		if (val == 0xe240f01f) {
			unsigned long offset, flags;

			// xxxxc:       e3e00a0f        mvn     r0, #61440      ; 0xf000
			// xxxx0:       e1a0e00f        mov     lr, pc
			// xxxx4:       e240f01f        sub     pc, r0, #31     ; 0x1f
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 8))))
				goto trap_and_emulate;
			if (val != 0xe1a0e00f)
				goto trap_and_emulate;
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 12))))
				goto trap_and_emulate;
			if (val != 0xe3e00a0f)
				goto trap_and_emulate;

			if (unlikely(((regs->ARM_lr - 4) & L4_PAGEMASK) != ((regs->ARM_lr - 12) & L4_PAGEMASK)))
				goto trap_and_emulate;

			val = parse_ptabs_read(regs->ARM_lr - 12, &offset, &flags);
			if (val == -EFAULT)
				goto trap_and_emulate;
			val += offset;

			*(unsigned long *)(val + 0) = 0xe3a00103; // mov r0, #0xc0000000
			*(unsigned long *)(val + 8) = 0xe240f020; // sub pc, r0, #32;

			local_irq_restore(flags);

			l4_cache_coherent(val, val + 12);

			regs->ARM_pc = regs->ARM_lr - 12;
			if (USER_PATCH_GETTLS_SHOW)
				printk("  handled (2)\n");
			return 1; // handled
		}

trap_and_emulate:
		if (USER_PATCH_GETTLS_SHOW)
			printk("   failed... emulating get-tls-func lr=%lx\n", regs->ARM_lr);
#endif
		regs->ARM_r0 = current_thread_info()->tp_value[0];
		regs->ARM_pc = regs->ARM_lr;
#ifdef CONFIG_ARM_THUMB
		if (regs->ARM_lr & 1)
			regs->ARM_cpsr |= PSR_T_BIT;
#endif
		return 1; // handled
	}

	if (regs->ARM_pc == 0xffff0fc0 && l4x_l4pfa(t) == 0xffff0fc0) {
		asmlinkage int arm_syscall(int no, struct pt_regs *regs);
#ifdef USER_PATCH_CMPXCHG
		unsigned long val;

		if (USER_PATCH_CMPXCHG_SHOW) {
			printk("CMPXCHG hit at lr=%lx pc=%lx\n", regs->ARM_lr, regs->ARM_pc);
			fiasco_tbuf_log_3val("cmpxchg", regs->ARM_lr, regs->ARM_pc, 0);
#ifdef CONFIG_L4_DEBUG_SEGFAULTS
			l4x_print_vm_area_maps(current, regs->ARM_pc);
#endif
		}

		if (regs->ARM_lr & 1)
			goto trap_and_emulate_cmpxchg;

		if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 4))))
			goto trap_and_emulate_cmpxchg;

		if (val == 0xe243f03f) {
			unsigned long offset, flags;

			// 2fca0:       e3e03a0f        mvn     r3, #61440      ; 0xf000
			// 2fca4:       e1a0e00f        mov     lr, pc
			// 2fca8:       e243f03f        sub     pc, r3, #63     ; 0x3f
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 8))))
				goto trap_and_emulate_cmpxchg;
			if (val != 0xe1a0e00f)
				goto trap_and_emulate_cmpxchg;
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 12))))
				goto trap_and_emulate_cmpxchg;
			if (val != 0xe3e03a0f)
				goto trap_and_emulate_cmpxchg;

			if (unlikely(((regs->ARM_lr - 4) & L4_PAGEMASK) != ((regs->ARM_lr - 12) & L4_PAGEMASK)))
				goto trap_and_emulate_cmpxchg;

			val = parse_ptabs_read(regs->ARM_lr - 12, &offset, &flags);
			if (val == -EFAULT)
				goto trap_and_emulate_cmpxchg;
			val += offset;

			*(unsigned long *)(val + 0) = 0xe3a03103;  // mov r3, #0xc0000000
			*(unsigned long *)(val + 8) = 0xe243f040;  // sub pc, r3, #64

			l4_cache_coherent(val, val + 12);

			local_irq_restore(flags);

			regs->ARM_pc = regs->ARM_lr - 12;
			if (USER_PATCH_CMPXCHG_SHOW)
				printk("   patched\n");
			return 1; // handled
		}


trap_and_emulate_cmpxchg:
		if (USER_PATCH_CMPXCHG_SHOW)
			printk("   failed... still taking the cmpxchg hit lr=%lx\n", regs->ARM_lr);
#endif
		if (0) {
			regs->ARM_r0 = arm_syscall(0xfff0 | __ARM_NR_BASE, regs);
			regs->ARM_pc = regs->ARM_lr;
#ifdef CONFIG_ARM_THUMB
			if (regs->ARM_lr & 1)
				regs->ARM_cpsr |= PSR_T_BIT;
#endif
		} else
			regs->ARM_pc = 0xbfffffc0;

		return 1; // handled
	}

	if (regs->ARM_pc == 0xffff0fa0 && l4x_l4pfa(t) == 0xffff0fa0) {
#ifdef USER_PATCH_DMB
		// dmb
		unsigned long val;

		if (USER_PATCH_DMB_SHOW) {
			printk("DMB hit at lr=%lx pc=%lx\n", regs->ARM_lr, regs->ARM_pc);
			fiasco_tbuf_log_3val("ARMdmb", regs->ARM_lr, regs->ARM_pc, 0);
		}

		if (regs->ARM_lr & 1)
			goto trap_and_emulate_dmb;

		if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 4))))
			goto trap_and_emulate_dmb;

		if (val == 0xe24cf05f) {
			unsigned long offset, flags;

			// xxxxc:       e3e0ca0f        mvn     ip, #61440      ; 0xf000
			// xxxx0:       e1a0e00f        mov     lr, pc
			// xxxx4:       e24cf05f        sub     pc, ip, #95     ; 0x5f
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 8))))
				goto trap_and_emulate_dmb;
			if (val != 0xe1a0e00f)
				goto trap_and_emulate_dmb;
			if (unlikely(get_user(val, (unsigned long *)(regs->ARM_lr - 12))))
				goto trap_and_emulate_dmb;
			if (val != 0xe3e0ca0f)
				goto trap_and_emulate_dmb;

			if (unlikely(((regs->ARM_lr - 4) & L4_PAGEMASK) != ((regs->ARM_lr - 12) & L4_PAGEMASK)))
				goto trap_and_emulate_dmb;

			val = parse_ptabs_read(regs->ARM_lr - 12, &offset, &flags);
			if (val == -EFAULT)
				goto trap_and_emulate_dmb;
			val += offset;

			*(unsigned long *)(val + 0) = 0xe3a0c103;  // mov ip, #0xc0000000
			*(unsigned long *)(val + 8) = 0xe24cf060;  // sub pc, ip, #96     ; 0x60

			l4_cache_coherent(val, val + 12);

			local_irq_restore(flags);

			regs->ARM_pc = regs->ARM_lr - 12;
			return 1; // handled
		}

trap_and_emulate_dmb:
		if (USER_PATCH_DMB_SHOW)
			printk("   failed... still taking the dmb hit lr=%lx\n", regs->ARM_lr);

#endif
		regs->ARM_pc = regs->ARM_lr & ~1;
#ifdef CONFIG_ARM_THUMB
		if (regs->ARM_lr & 1)
			regs->ARM_cpsr |= PSR_T_BIT;
#endif
		return 1; // handled
	}

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
