#ifndef __ASM_L4__L4X_ARM__EXCEPTION_H__
#define __ASM_L4__L4X_ARM__EXCEPTION_H__

#include <asm/ptrace.h>

#include <l4/sys/vcpu.h>

enum l4x_cpu_modes {
	L4X_MODE_KERNEL = PSR_MODE_EL1h,
	L4X_MODE_USER   = PSR_MODE_EL0t,
};

static inline void l4x_set_cpu_mode(struct pt_regs *r, enum l4x_cpu_modes mode)
{
	r->pstate = (r->pstate & ~PSR_MODE_MASK) | (PSR_MODE_MASK & mode);
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
	return processor_mode(r);
}

static inline void l4x_init_kernel_regs(struct pt_regs *r)
{
	r->pstate = PSR_MODE_EL1h;
}

static inline void l4x_set_regs_ip(struct pt_regs *r, unsigned long ip)
{
	instruction_pointer_set(r, ip);
}

#ifdef CONFIG_L4_VCPU
static inline void vcpu_to_ptregs_common(l4_vcpu_state_t *v,
                                         struct pt_regs *regs)
{
	memcpy(regs->regs, v->r.r, sizeof(long) * 31);
	regs->sp = v->r.sp;
	regs->pc = v->r.ip;
}

static inline void vcpu_to_ptregs_kern(l4_vcpu_state_t *v,
                                       struct pt_regs *regs)
{
	vcpu_to_ptregs_common(v, regs);
	if (v->saved_state & L4_VCPU_F_IRQ)
		regs->pstate = v->r.flags & ~PSR_I_BIT;
	else
		regs->pstate = v->r.flags | PSR_I_BIT;

	regs->pstate = (regs->pstate & ~PSR_MODE_MASK) | PSR_MODE_EL1h;
}

static inline void vcpu_to_ptregs_user(l4_vcpu_state_t *v,
                                       struct pt_regs *regs)
{
	vcpu_to_ptregs_common(v, regs);
	// disable IRQ and FIQ, for valid_user_regs(regsp) + mode
	regs->pstate = v->r.flags & ~(PSR_F_BIT | PSR_I_BIT | PSR_MODE_MASK);
}

static inline void ptregs_to_vcpu(l4_vcpu_state_t *v,
                                  struct pt_regs *regs)
{
	memcpy(v->r.r, regs->regs, sizeof(long) * 31);
	v->r.sp    = regs->sp;
	v->r.ip    = regs->pc;
	v->r.flags = regs->pstate & ((regs->pstate & ~PSR_MODE_MASK) | PSR_MODE_EL0t);
	v->saved_state &= ~(L4_VCPU_F_IRQ | L4_VCPU_F_USER_MODE);
	if (!(regs->pstate & PSR_I_BIT))
		v->saved_state |= L4_VCPU_F_IRQ;
	if ((regs->pstate & PSR_MODE_MASK) == PSR_MODE_EL0t)
		v->saved_state |= L4_VCPU_F_USER_MODE;
}
#endif

static inline void utcb_exc_to_ptregs(l4_exc_regs_t *exc, struct pt_regs *ptregs)
{
	memcpy(ptregs->regs, exc->r, sizeof(unsigned long) * 31);
	ptregs->sp     = exc->sp;
	ptregs->pc     = exc->pc;
	// disable IRQ and FIQ, for valid_user_regs(regsp) + mode
	ptregs->pstate = exc->pstate & ~(PSR_F_BIT | PSR_I_BIT | ~PSR_MODE_MASK);
}

static inline void ptregs_to_utcb_exc(struct pt_regs *ptregs, l4_exc_regs_t *exc)
{
	memcpy(exc->r, ptregs->regs, sizeof(unsigned long) * 31);
	exc->sp     = ptregs->sp;
	exc->pc     = ptregs->pc;
	exc->pstate = ptregs->pstate;
}

static inline
unsigned long l4x_thread_struct_error_code(struct thread_struct *t)
{
	return t->fault_code;
}

#define L4X_FAKE_EX_TABLE_ENTRY() ({ unsigned long pc;          \
	asm volatile ("1: adr	%0, 1b                    \n\t" \
	              ".pushsection \"__ex_table\",\"a\"  \n\t" \
	              ".align	3                         \n\t" \
	              ".long 1b, 1b                \n\t" \
	              ".popsection" : "=r"(pc)); pc; })

#endif /* ! __ASM_L4__L4X_ARM__EXCEPTION_H__ */
