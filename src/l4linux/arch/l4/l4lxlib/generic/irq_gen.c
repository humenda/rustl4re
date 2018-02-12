#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <l4/sys/irq.h>
#include <l4/sys/icu.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/io/io.h>
#include <l4/re/env.h>

#include <asm/generic/cap_alloc.h>

#include <asm/l4lxapi/generic/irq_gen.h>

struct l4x_irq_desc_private *
l4x_alloc_irq_desc_data(int lx_irq, l4_cap_idx_t icu, unsigned icu_irq)
{
	struct l4x_irq_desc_private *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	p->c.irq_cap = L4_INVALID_CAP;
	p->icu       = icu;

	irq_desc_get_irq_data(irq_to_desc(lx_irq))->hwirq = icu_irq;

	return p;
}

void l4x_free_irq_dest_data(struct l4x_irq_desc_private *p)
{
	kfree(p);
}

int l4lx_irq_set_type(struct irq_data *data, unsigned int type)
{
	unsigned int irq = data->irq;
	struct l4x_irq_desc_private *p;
	struct irq_desc *desc = irq_to_desc(irq);
	int r;

	if (unlikely(irq >= NR_IRQS))
		return -1;

	p = irq_data_get_irq_chip_data(data);
	if (!p)
		return -1;

	printk("L4IRQ: set irq type of %u to %x\n", irq, type);
	switch (type & IRQF_TRIGGER_MASK) {
		case IRQ_TYPE_EDGE_BOTH:
			p->trigger = L4_IRQ_F_BOTH_EDGE;
			desc->handle_irq = handle_edge_eoi_irq;
			break;
		case IRQ_TYPE_EDGE_RISING:
			p->trigger = L4_IRQ_F_POS_EDGE;
			desc->handle_irq = handle_edge_eoi_irq;
			break;
		case IRQ_TYPE_EDGE_FALLING:
			p->trigger = L4_IRQ_F_NEG_EDGE;
			desc->handle_irq = handle_edge_eoi_irq;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			p->trigger = L4_IRQ_F_LEVEL_HIGH;
			desc->handle_irq = handle_level_irq;
			break;
		case IRQ_TYPE_LEVEL_LOW:
			p->trigger = L4_IRQ_F_LEVEL_LOW;
			desc->handle_irq = handle_level_irq;
			break;
		default:
			p->trigger = L4_IRQ_F_NONE;
			break;
	};

	if (   l4_is_invalid_cap(p->c.irq_cap)
	    || l4_is_invalid_cap(p->icu))
		return 0;

	r = L4XV_FN_e(l4_icu_set_mode(p->icu, data->hwirq, p->trigger));
	if (r)
		pr_err("l4x-irq: l4-set-mode(%d) failed for IRQ %d: %d\n",
		       type, irq, r);

	return r;
}

int l4x_irq_release(l4_cap_idx_t irqcap)
{
	return L4XV_FN_e(l4_task_unmap(L4_BASE_TASK_CAP,
	                               l4_obj_fpage(irqcap, 0, L4_FPAGE_RWX),
	                               L4_FP_ALL_SPACES));
}

l4_cap_idx_t l4x_irq_init(l4_cap_idx_t icu, unsigned icu_irq, unsigned trigger,
                          char *tag)
{
	l4_cap_idx_t cap;

	if (l4_is_invalid_cap(cap = l4x_cap_alloc()))
		return L4_INVALID_CAP;

	if (L4XV_FN_e(l4_icu_set_mode(icu, icu_irq, trigger)) < 0) {
		pr_err("l4x: Failed to set type for ICU-IRQ %s:%d\n",
		       tag, icu_irq);
		goto out1;
	}

	if (L4XV_FN_e(l4_factory_create_irq(l4re_env()->factory, cap))) {
		pr_err("l4x: Failed to create IRQ for ICU-IRQ %s:%d\n",
		       tag, icu_irq);
		goto out1;
	}

	if (L4XV_FN_e(l4_icu_bind(icu, icu_irq, cap))) {
		pr_err("l4x: Failed to bind ICU-IRQ %s:%d\n", tag, icu_irq);
		goto out2;
	}

	return cap;

out2:
	l4x_irq_release(cap);
out1:
	l4x_cap_free(cap);
	return L4_INVALID_CAP;
}

void l4x_irq_deinit(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	if (L4XV_FN_e(l4_icu_unbind(p->icu, data->hwirq, p->c.irq_cap)))
		pr_err("l4x-irq: Failed to unbind IRQ %ld\n", data->hwirq);

	if (l4x_irq_release(p->c.irq_cap))
		pr_err("l4x-irq: Failed to release IRQ cap %lx\n",
		       p->c.irq_cap);

	l4x_cap_free(p->c.irq_cap);
	p->c.irq_cap = L4_INVALID_CAP;
}
