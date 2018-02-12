
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/msi.h>

#include <asm/ptrace.h>
#include <asm/irq.h>
#include <asm/generic/irq.h>
#include <asm/l4lxapi/irq.h>

#include <l4/sys/types.h>
#include <l4/sys/icu.h>
#include <l4/io/io.h>

int __init arch_probe_nr_irqs(void)
{
	BUILD_BUG_ON(L4X_NR_IRQS_V_DYN < 1);

	return NR_IRQS_HW;
}

int l4x_init_dyn_irq(int lx_irq, l4_cap_idx_t irqcap, l4_cap_idx_t icucap,
                     unsigned icu_irq, struct irq_chip *chip)
{
	int ret;
	struct l4x_irq_desc_private *p;

	p = l4x_alloc_irq_desc_data(lx_irq, icucap, icu_irq);
	if (!p)
		return -EINVAL;

	ret = irq_set_chip_data(lx_irq, p);
	if (ret < 0)
		return ret;

	p->c.irq_cap = irqcap;

	irq_set_chip_and_handler(lx_irq, chip, handle_edge_eoi_irq);

	return 0;
}

int l4x_register_irq(l4_cap_idx_t irqcap)
{
	int irq, ret;

	irq = irq_alloc_desc_from(L4X_IRQS_V_DYN_BASE, -1);
	if (irq < 0)
		return irq;

	BUG_ON(irq < L4X_IRQS_V_DYN_BASE);

	ret = l4x_init_dyn_irq(irq, irqcap, L4_INVALID_CAP,
	                       irq, &l4x_irq_plain_chip);
	if (ret)
		return ret;

	return irq;
}

int l4x_alloc_percpu_irq(l4_cap_idx_t __percpu **percpu_caps)
{
	struct l4x_irq_desc_private *p;
	int irq = l4x_register_irq(L4_INVALID_CAP);

	if (irq < 0)
		return irq;

	p = irq_get_chip_data(irq);

	p->c.irq_caps = alloc_percpu(l4_cap_idx_t);
	p->is_percpu = 1;

	irq_set_percpu_devid(irq);
	irq_set_chip_and_handler(irq, &l4x_irq_plain_chip, handle_percpu_devid_irq);

	*percpu_caps = p->c.irq_caps;

	return irq;
}

void l4x_free_percpu_irq(int irq)
{
	int i = irq - L4X_IRQS_V_DYN_BASE;
	struct l4x_irq_desc_private *p = irq_get_chip_data(irq);

	BUG_ON(irq < L4X_IRQS_V_DYN_BASE);
	BUG_ON(i >= L4X_NR_IRQS_V_DYN);

	free_percpu(p->c.irq_caps);

	l4x_unregister_irq(irq);
}

int l4x_register_percpu_irqcap(int irqnum, unsigned cpu, l4_cap_idx_t cap)
{
	struct l4x_irq_desc_private *p = irq_get_chip_data(irqnum);

	if (!p)
		return -EINVAL;

	*per_cpu_ptr(p->c.irq_caps, cpu) = cap;

	return 0;
}

void l4x_unregister_irq(int irqnum)
{
	irq_free_desc(irqnum);
}

l4_cap_idx_t l4x_have_irqcap(int irqnum, unsigned cpu)
{
	struct l4x_irq_desc_private *p = irq_get_chip_data(irqnum);

	if (!p || cpu >= NR_CPUS)
		return L4_INVALID_CAP;

	if (p->is_percpu)
		return *per_cpu_ptr(p->c.irq_caps, cpu);

	return p->c.irq_cap;
}

static int l4lx_irq_plain_set_wake(struct irq_data *data, unsigned int on)
{
	struct irq_desc *desc = irq_to_desc(data->irq);

	if (on)
		desc->action->flags |= IRQF_NO_SUSPEND;
	else
		desc->action->flags &= ~IRQF_NO_SUSPEND;

	return 0;
}

static int l4lx_irq_icu_set_wake(struct irq_data *data, unsigned int on)
{
	unsigned m = on ? L4_IRQ_F_SET_WAKEUP : L4_IRQ_F_CLEAR_WAKEUP;

	if (L4XV_FN_e(l4_icu_set_mode(l4io_request_icu(), data->irq, m)) < 0) {
		pr_err("l4x-irq: Failed to set wakeup type for IRQ %d\n",
		       data->irq);
		return -EINVAL;
	}

	l4lx_irq_plain_set_wake(data, on);
	return 0;
}

struct irq_chip l4x_irq_icu_chip = {
	.name                   = "L4-icu",
	.irq_startup            = l4lx_irq_icu_startup,
	.irq_shutdown           = l4lx_irq_icu_shutdown,
	.irq_enable             = l4lx_irq_dev_enable,
	.irq_disable            = l4lx_irq_dev_disable,
	.irq_ack                = l4lx_irq_dev_ack,
	.irq_mask               = l4lx_irq_dev_mask,
	.irq_mask_ack           = l4lx_irq_dev_mask_ack,
	.irq_unmask             = l4lx_irq_dev_unmask,
	.irq_eoi                = l4lx_irq_dev_eoi,
	.irq_set_type           = l4lx_irq_set_type,
	.irq_set_wake           = l4lx_irq_icu_set_wake,
#ifdef CONFIG_L4_VCPU
	.irq_set_affinity       = l4lx_irq_dev_set_affinity,
#endif
};

struct irq_chip l4x_irq_plain_chip = {
	.name                   = "L4-plain",
	.irq_startup            = l4lx_irq_plain_startup,
	.irq_shutdown           = l4lx_irq_plain_shutdown,
	.irq_enable             = l4lx_irq_dev_enable,
	.irq_disable            = l4lx_irq_dev_disable,
	.irq_ack                = l4lx_irq_dev_ack,
	.irq_mask               = l4lx_irq_dev_mask,
	.irq_mask_ack           = l4lx_irq_dev_mask_ack,
	.irq_unmask             = l4lx_irq_dev_unmask,
	.irq_eoi                = l4lx_irq_dev_eoi,
	.irq_set_type           = l4lx_irq_set_type,
	.irq_set_wake           = l4lx_irq_plain_set_wake,
#ifdef CONFIG_L4_VCPU
	.irq_set_affinity       = l4lx_irq_dev_set_affinity,
#endif
};
