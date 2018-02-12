#include <asm/io.h>

#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/irq.h>

#include <l4/sys/irq.h>
#include <l4/sys/icu.h>

#include <asm/api/config.h>
#include <asm/api/macros.h>

#include <asm/l4lxapi/irq.h>
#include <asm/l4lxapi/thread.h>
#include <asm/l4lxapi/misc.h>

#include <asm/generic/io.h>
#include <asm/generic/sched.h>
#include <asm/generic/setup.h>
#include <asm/generic/task.h>
#include <asm/generic/do_irq.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/stack_id.h>
#include <asm/generic/smp.h>
#include <asm/generic/irq.h>
#include <asm/generic/jdb.h>

#include <l4/sys/debugger.h>

#define d_printk(format, args...)  printk(format , ## args)
#define dd_printk(format, args...) do { if (0) printk(format , ## args); } while (0)

static DEFINE_SPINLOCK(migrate_lock);

static inline l4_cap_idx_t get_irq(struct l4x_irq_desc_private *p)
{
	return p->is_percpu ? *this_cpu_ptr(p->c.irq_caps) : p->c.irq_cap;
}

static inline void attach_to_irq(struct irq_data *data)
{
	long r;
	unsigned irq = data->irq;
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);
	unsigned cpu = p->is_percpu ? smp_processor_id() : p->cpu;
	l4_cap_idx_t ic = get_irq(p);

	if ((r = L4XV_FN_e(l4_rcv_ep_bind_thread(ic,
	                                         l4x_cpu_thread_get_cap(cpu),
	                                         irq << 2))))
		dd_printk("%s: can't register to irq %u: return=%ld\n",
		          __func__, irq, r);

	if (p->is_percpu)
		L4XV_FN_v(l4x_dbg_set_object_name(ic, "irq%u/%u", irq, cpu));
	else
		L4XV_FN_v(l4x_dbg_set_object_name(ic, "irq%u", irq));

}

static void detach_from_interrupt(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);
	unsigned long flags;

	local_irq_save(flags);
	if (l4_error(l4_irq_detach(get_irq(p))))
		dd_printk("%02d: Unable to detach from IRQ\n", data->irq);
	local_irq_restore(flags);
}

void l4lx_irq_init(void)
{
}

unsigned int l4lx_irq_icu_startup(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	BUG_ON(p->is_percpu);
	BUG_ON(!l4_is_invalid_cap(p->c.irq_cap));

	p->c.irq_cap = l4x_irq_init(p->icu, data->hwirq, p->trigger, "icu");
	if (l4_is_invalid_cap(p->c.irq_cap)) {
		pr_err("l4x-irq: Failed to initialize IRQ %ld\n", data->hwirq);
		return -ENXIO;
	}

	l4lx_irq_dev_enable(data);
	return 0;
}

unsigned int l4lx_irq_msi_startup(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	BUG_ON(p->is_percpu);
	BUG_ON(l4_is_invalid_cap(p->c.irq_cap));

	l4lx_irq_dev_enable(data);
	irq_data_get_irq_chip(data)->irq_unmask(data);
	return 0;
}

unsigned int l4lx_irq_plain_startup(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);
	p->c.irq_cap = l4x_have_irqcap(data->irq, smp_processor_id());
	BUG_ON(l4_is_invalid_cap(p->c.irq_cap));
	l4lx_irq_dev_enable(data);
	return 0;
}

void l4lx_irq_icu_shutdown(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
	l4lx_irq_dev_disable(data);
	l4x_irq_deinit(data);
}


void l4lx_irq_msi_shutdown(struct irq_data *data)
{
	unsigned irq = data->irq;

	dd_printk("%s: %u\n", __func__, irq);
	l4lx_irq_dev_disable(data);
}

void l4lx_irq_plain_shutdown(struct irq_data *data)
{
}

void l4lx_irq_dev_enable(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	dd_printk("%s: %u\n", __func__, data->irq);

	if (unlikely(!p->is_percpu && l4_is_invalid_cap(p->c.irq_cap))) {
		WARN_ONCE(1, "Use of irq-enable on non-initialized IRQ%d",
		          data->irq);
		return;
	}

	if (p->is_percpu || !p->enabled) {
		p->enabled = 1;
		attach_to_irq(data);
	}
	l4lx_irq_dev_eoi(data);
}

void l4lx_irq_dev_disable(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	if (unlikely(!p->is_percpu && l4_is_invalid_cap(p->c.irq_cap))) {
		WARN_ONCE(1, "Use of irq-disable on non-initialized IRQ%d",
		          data->irq);
		return;
	}

	dd_printk("%s: %u\n", __func__, data->irq);

	/* do not detach if the IRQ is used for wakeup */
	if (irqd_is_wakeup_set(data)) {
		pr_info("leave IRQ %d attached, is configured as wakeup\n",
		        data->irq);
		return;
	}

	if (p->is_percpu || p->enabled) {
		p->enabled = 0;
		detach_from_interrupt(data);
	}
}

void l4lx_irq_dev_ack(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
}

void l4lx_irq_dev_mask(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
}

void l4lx_irq_dev_mask_ack(struct irq_data *data)
{
	dd_printk("%s: %u\n", __func__, data->irq);
}

void l4lx_irq_dev_unmask(struct irq_data *data)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);
	unsigned long flags;

	if (unlikely(l4_is_invalid_cap(get_irq(p)))) {
		WARN_ONCE(1, "Use of irq-unmask on non-initialized IRQ%d",
		          data->irq);
		return;
	}

	irq_move_irq(data);


	dd_printk("%s: %u\n", __func__, data->irq);
	local_irq_save(flags);
	l4_irq_unmask(get_irq(p));
	local_irq_restore(flags);
}

void l4lx_irq_dev_eoi(struct irq_data *data)
{
	l4lx_irq_dev_unmask(data);
}

int
do_l4lx_irq_set_affinity(struct irq_data *data,
                         const struct cpumask *dest, bool force,
                         int (*fn)(struct irq_data *data,
                                   unsigned logical_dest))
{
	unsigned target_cpu;
	unsigned long flags;
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	if (p->is_percpu)
		return IRQ_SET_MASK_OK_NOCOPY;

	if (unlikely(l4_is_invalid_cap(p->c.irq_cap)))
		return IRQ_SET_MASK_OK_NOCOPY;

	if (!cpumask_intersects(dest, cpu_online_mask))
                return IRQ_SET_MASK_OK_NOCOPY;

        target_cpu = cpumask_any_and(dest, cpu_online_mask);

	if (target_cpu == p->cpu)
		return IRQ_SET_MASK_OK_NOCOPY;

	spin_lock_irqsave(&migrate_lock, flags);

	if (fn) {
		int ret = fn(data, target_cpu);
		if (ret < 0)
			return ret;
	}

	cpumask_copy(irq_data_get_affinity_mask(data), dest);
	p->cpu = target_cpu;
	if (p->enabled) {
		attach_to_irq(data);
		l4_irq_unmask(p->c.irq_cap);
	}
	spin_unlock_irqrestore(&migrate_lock, flags);

	return IRQ_SET_MASK_OK_NOCOPY;
}

int l4lx_irq_dev_set_affinity(struct irq_data *data,
                              const struct cpumask *dest, bool force)
{
	return do_l4lx_irq_set_affinity(data, dest, force, NULL);
}
