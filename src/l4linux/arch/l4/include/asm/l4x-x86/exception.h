#ifndef __ASM_L4__L4X_I386__EXCEPTION_H__
#define __ASM_L4__L4X_I386__EXCEPTION_H__

#include <asm/ptrace.h>

#include <l4/sys/utcb.h>

enum l4x_cpu_modes {
	L4X_MODE_KERNEL = 0,
	L4X_MODE_USER   = USER_RPL,
};

static inline void l4x_set_cpu_mode(struct pt_regs *r, enum l4x_cpu_modes mode)
{
	r->cs = mode;
}

static inline void l4x_set_user_mode(struct pt_regs *r)
{
	l4x_set_cpu_mode(r, L4X_MODE_USER);
}

static inline void l4x_set_kernel_mode(struct pt_regs *r)
{
	l4x_set_cpu_mode(r, L4X_MODE_KERNEL);
}

static inline unsigned long l4x_get_cpu_mode(struct pt_regs *r)
{
	return r->cs & SEGMENT_RPL_MASK;
}

static inline void l4x_init_kernel_regs(struct pt_regs *r)
{
	l4x_set_cpu_mode(r, L4X_MODE_KERNEL);
	r->flags = X86_EFLAGS_IF;
}

static inline void l4x_set_regs_ip(struct pt_regs *r, unsigned long ip)
{
	r->ip = ip;
}

#ifdef CONFIG_L4_VCPU
static inline void vcpu_to_ptregs_common(l4_vcpu_state_t *v,
                                         struct pt_regs *r)
{
	r->ax    = v->r.ax;
	r->bx    = v->r.bx;
	r->cx    = v->r.cx;
	r->dx    = v->r.dx;
	r->di    = v->r.di;
	r->si    = v->r.si;
	r->bp    = v->r.bp;
	r->ip    = v->r.ip;
	r->flags = v->r.flags;
	r->sp    = v->r.sp;
#ifdef CONFIG_X86_32
	r->fs    = v->r.fs;
#else
	r->cs    = v->r.cs;
	r->r8    = v->r.r8;
	r->r9    = v->r.r9;
	r->r10   = v->r.r10;
	r->r11   = v->r.r11;
	r->r12   = v->r.r12;
	r->r13   = v->r.r13;
	r->r14   = v->r.r14;
	r->r15   = v->r.r15;
#endif
	if (v->saved_state & L4_VCPU_F_IRQ)
	        r->flags |= X86_EFLAGS_IF;
	else
	        r->flags &= ~X86_EFLAGS_IF;
}

static inline void vcpu_to_ptregs_kern(l4_vcpu_state_t *v,
                                       struct pt_regs *regs)
{
	vcpu_to_ptregs_common(v, regs);
#ifdef CONFIG_X86_64
	regs->cs = regs->cs & ~SEGMENT_RPL_MASK;
#else
	regs->cs = 0;
#endif
}

static inline void vcpu_to_ptregs_user(l4_vcpu_state_t *v,
                                       struct pt_regs *regs)
{
	vcpu_to_ptregs_common(v, regs);
#ifdef CONFIG_X86_64
	regs->cs = (regs->cs & ~SEGMENT_RPL_MASK) | USER_RPL;
#endif
}

static inline void ptregs_to_vcpu(l4_vcpu_state_t *v,
                                  struct pt_regs *r)
{
	v->r.ax    = r->ax;
	v->r.bx    = r->bx;
	v->r.cx    = r->cx;
	v->r.dx    = r->dx;
	v->r.di    = r->di;
	v->r.si    = r->si;
	v->r.bp    = r->bp;
	v->r.ip    = r->ip;
	v->r.flags = r->flags;
	v->r.sp    = r->sp;
#ifdef CONFIG_X86_32
	v->r.fs    = r->fs;
#else
	v->r.cs    = r->cs | 3;
	v->r.r8    = r->r8;
	v->r.r9    = r->r9;
	v->r.r10   = r->r10;
	v->r.r11   = r->r11;
	v->r.r12   = r->r12;
	v->r.r13   = r->r13;
	v->r.r14   = r->r14;
	v->r.r15   = r->r15;
#endif
	v->saved_state &= ~(L4_VCPU_F_IRQ | L4_VCPU_F_USER_MODE);
	if (r->flags & X86_EFLAGS_IF)
	        v->saved_state |= L4_VCPU_F_IRQ;
	if (r->cs & 3)
	        v->saved_state |= L4_VCPU_F_USER_MODE;
}

#endif

#ifdef CONFIG_X86_32
#define RN(n) e##n
#else
#define RN(n) r##n
#endif

static inline
void utcb_exc_to_ptregs(l4_exc_regs_t *exc, struct pt_regs *ptregs)
{
	ptregs->ax    = exc->RN(ax);
	ptregs->bx    = exc->RN(bx);
	ptregs->cx    = exc->RN(cx);
	ptregs->dx    = exc->RN(dx);
	ptregs->di    = exc->RN(di);
	ptregs->si    = exc->RN(si);
	ptregs->bp    = exc->RN(bp);
	ptregs->ip    = exc->ip;
	ptregs->flags = exc->flags | X86_EFLAGS_IF;
	ptregs->sp    = exc->sp;
	ptregs->ss    = exc->ss;
#ifdef CONFIG_X86_32
	ptregs->fs    = exc->fs;
	ptregs->gs    = exc->gs;
#else
	ptregs->r8    = exc->r8;
	ptregs->r9    = exc->r9;
	ptregs->r10   = exc->r10;
	ptregs->r11   = exc->r11;
	ptregs->r12   = exc->r12;
	ptregs->r13   = exc->r13;
	ptregs->r14   = exc->r14;
	ptregs->r15   = exc->r15;
#endif
}

static inline
void ptregs_to_utcb_exc(struct pt_regs *ptregs, l4_exc_regs_t *exc)
{
	exc->RN(ax) = ptregs->ax;
	exc->RN(bx) = ptregs->bx;
	exc->RN(cx) = ptregs->cx;
	exc->RN(dx) = ptregs->dx;
	exc->RN(di) = ptregs->di;
	exc->RN(si) = ptregs->si;
	exc->RN(bp) = ptregs->bp;
	exc->ip     = ptregs->ip;
	exc->flags  = ptregs->flags;
	exc->sp     = ptregs->sp;
	exc->ss     = ptregs->ss;
#ifdef CONFIG_X86_32
	exc->fs     = ptregs->fs;
#else
	exc->r8     = ptregs->r8;
	exc->r9     = ptregs->r9;
	exc->r10    = ptregs->r10;
	exc->r11    = ptregs->r11;
	exc->r12    = ptregs->r12;
	exc->r13    = ptregs->r13;
	exc->r14    = ptregs->r14;
	exc->r15    = ptregs->r15;
#endif
}

#define L4X_FAKE_EX_TABLE_ENTRY() ({ unsigned long pc;               \
	asm volatile ("1: mov $1b, %0                          \n\t" \
	              ".pushsection \"__ex_table\",\"a\"       \n\t" \
	              ".balign 4                               \n\t" \
	              ".long 1b - .; .long 1b - .; .long 0 - . \n\t" \
	              ".popsection" : "=r"(pc)); pc; })

#endif /* ! __ASM_L4__L4X_I386__EXCEPTION_H__ */
