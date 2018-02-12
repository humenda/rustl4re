#ifndef __ASM_L4__GENERIC__VCPU_H__
#define __ASM_L4__GENERIC__VCPU_H__

#include <l4/sys/vcpu.h>
#include <linux/threads.h>

extern l4_vcpu_state_t *l4x_vcpu_ptr[NR_CPUS];

#define l4x_current_vcpu() (l4x_vcpu_ptr[smp_processor_id()])

#ifdef CONFIG_L4_VCPU

#include <linux/irqflags.h>
#include <linux/threads.h>
#include <linux/linkage.h>
#include <asm/ptrace.h>

#define  L4XV_V(n) unsigned long n
#define  L4XV_L(n) local_irq_save(n)
#define  L4XV_U(n) local_irq_restore(n)

void l4x_vcpu_handle_irq(l4_vcpu_state_t *t, struct pt_regs *regs);
void l4x_vcpu_handle_ipi(struct pt_regs *regs);
void l4x_vcpu_entry(l4_vcpu_state_t *vcpu);

static inline void l4x_vcpu_init(l4_vcpu_state_t *v)
{
	v->state     = L4_VCPU_F_EXCEPTIONS;
	v->entry_ip  = (l4_addr_t)&l4x_vcpu_entry;
	v->user_task = L4_INVALID_CAP;
}

static inline
int l4x_is_vcpu(void)
{
	return 1;
}

#else

static inline
int l4x_is_vcpu(void)
{
	return 0;
}

#define  L4XV_V(n)
#define  L4XV_L(n) do {} while (0)
#define  L4XV_U(n) do {} while (0)

#endif

/* Generic variant */
#define  L4XV_FN(rettype, fn) \
	 ({ rettype __r; L4XV_V(__f); L4XV_L(__f); \
	  __r = fn; L4XV_U(__f); __r; })

/* Often used: */
#define  L4XV_FN_i(fn) L4XV_FN(int, fn)
#define  L4XV_FN_l(fn) L4XV_FN(long, fn)
#define  L4XV_FN_ui(fn) L4XV_FN(unsigned int, fn)
#define  L4XV_FN_ul(fn) L4XV_FN(unsigned long, fn)

#define  L4XV_FN_v(fn) \
	 ({ L4XV_V(__f); L4XV_L(__f); fn; L4XV_U(__f); })

#define  L4XV_FN_e(fn) L4XV_FN_l(l4_error(fn))

#endif /* ! __ASM_L4__GENERIC__VCPU_H__ */
