#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/msi.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>

#include <asm/ptrace.h>
#include <asm/irq.h>
#if defined(CONFIG_X86)
#include <asm/irqdomain.h>
#ifdef CONFIG_SMP
#include <asm/idtentry.h>
#endif
#endif
#include <asm/generic/irq.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/smp_ipi.h>
#include <asm/generic/util.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/setup.h>
#include <asm/generic/jdb.h>

#include <asm/l4lxapi/irq.h>
#include <asm/l4lxapi/thread.h>

#include <l4/sys/types.h>
#include <l4/sys/icu.h>
#include <l4/sys/irq.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/io/io.h>
#include <l4/util/atomic.h>
#include <l4/util/util.h>
#include <l4/re/env.h>

#include <asm/generic/log.h>

static l4_umword_t l4x_cpu_ipi_vector_mask[NR_CPUS];
static l4_cap_idx_t l4x_cpu_ipi_irqs[NR_CPUS];

struct irq_domain *l4x_irq_domain;

int l4x_ipi_irq;
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
#ifdef CONFIG_SMP
static struct irq_domain *ipi_domain;
#endif /* SMP */
static unsigned l4x_of_found_l4icu;
#endif /* ARM-both */

#if !defined(CONFIG_L4_VCPU) && defined(CONFIG_SMP)
static l4lx_thread_t l4x_cpu_ipi_threads[NR_CPUS];
#endif /* !VCPU && SMP */

enum { IPI_FUNCS = 8 };

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

static int l4x_irq_domain_map(struct irq_domain *d, unsigned int irq,
                              irq_hw_number_t hw)
{
	return 0;
}

static int l4x_irq_domain_alloc(struct irq_domain *domain, unsigned int virq,
                                unsigned int nr_irqs, void *arg)
{
	struct irq_data *irqd;
	int i;

	for (i = 0; i < nr_irqs; i++) {
		irqd = irq_domain_get_irq_data(domain, virq + i);

		irqd->chip = &l4x_irq_plain_chip;
		irqd->chip_data = NULL;
		irqd->hwirq = virq + i;
		irqd_set_single_target(irqd);
	}


	return 0;
}

static void l4x_irq_domain_free(struct irq_domain *domain,
                                unsigned int virq, unsigned int nr_irqs)
{
}

static struct irq_domain_ops l4x_irq_domain_ops = {
	.map = l4x_irq_domain_map,
	.alloc = l4x_irq_domain_alloc,
	.free  = l4x_irq_domain_free,
};

int l4x_register_irq(l4_cap_idx_t irqcap)
{
	int irq, ret;

	irq = irq_create_direct_mapping(l4x_irq_domain);
	if (irq == 0)
		return -ENOMEM;
	if (irq < 0)
		return irq;

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
	struct l4x_irq_desc_private *p = irq_get_chip_data(irq);
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

static int l4lx_irq_retrigger(struct irq_data *data)
{
	printk("l4lx_irq_retrigger: %ld\n", data->hwirq);
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
	.irq_retrigger          = l4lx_irq_retrigger,
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
	.irq_retrigger          = l4lx_irq_retrigger,
#ifdef CONFIG_L4_VCPU
	.irq_set_affinity       = l4lx_irq_dev_set_affinity,
#endif
};

#ifdef CONFIG_SMP
#ifndef CONFIG_L4_VCPU
#include <asm/generic/sched.h>
#include <asm/generic/do_irq.h>
#include <l4/sys/irq.h>
static __noreturn L4_CV void l4x_cpu_ipi_thread(void *x)
{
	unsigned cpu = *(unsigned *)x;
	l4_msgtag_t tag;
	l4_cap_idx_t cap;
	int err;
	l4_utcb_t *utcb = l4_utcb();

	l4x_prepare_irq_thread(current_stack_pointer, cpu);

	// wait until parent has setup our cap and attached us
	while (l4_is_invalid_cap(l4x_cpu_ipi_irqs[cpu])) {
		mb();
		l4_sleep(2);
	}

	cap = l4x_cpu_ipi_irqs[cpu];

	while (1) {
		tag = l4_irq_receive(cap, L4_IPC_NEVER);
		if (unlikely(err = l4_ipc_error(tag, utcb)))
			l4x_printf("ipi%d: ipc error %d (cap: %lx)\n",
			           cpu, err, cap);

		while (1) {
			l4_umword_t ret, mask;
			unsigned vector;
			do {
				mask = l4x_cpu_ipi_vector_mask[cpu];
				ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
				                     mask, 0);
			} while (unlikely(!ret));

			if (!mask)
				break;

			while (mask) {
				vector = ffs(mask) - 1;
				l4x_do_IPI(vector);
				mask &= ~(1 << vector);
			}
		}
	}
}
#endif /* !VCPU */

void l4x_cpu_ipi_setup(unsigned cpu)
{
	l4_msgtag_t t;
	l4_cap_idx_t c;
	char s[14];

#ifdef CONFIG_L4_VCPU
	BUG_ON(l4x_vcpu_ptr[cpu]->state & L4_VCPU_F_IRQ);
#endif

	snprintf(s, sizeof(s), "ipi%d", cpu);
	s[sizeof(s) - 1] = 0;

	l4x_cpu_ipi_irqs[cpu] = L4_INVALID_CAP;

#ifndef CONFIG_L4_VCPU
	if (l4lx_thread_create(&l4x_cpu_ipi_threads[cpu],
	                       l4x_cpu_ipi_thread,
	                       cpu,
	                       NULL, &cpu, sizeof(cpu),
	                       l4x_cap_alloc(),
	                       l4lx_irq_prio_get(0), 0, 0, s, NULL))
		l4x_exit_l4linux_msg("Failed to create thread %s\n", s);
#endif

	c = l4x_cap_alloc_noctx();
	if (l4_is_invalid_cap(c))
		l4x_exit_l4linux_msg("Failed to alloc cap\n");

	t = l4_factory_create_irq(l4re_env()->factory, c);
	if (l4_error(t))
		l4x_exit_l4linux_msg("Failed to create IRQ\n");

	l4x_dbg_set_object_name(c, "ipi%d", cpu);

	BUG_ON(l4x_ipi_irq == 0);

#ifdef CONFIG_L4_VCPU
	t = l4_rcv_ep_bind_thread(c, l4lx_thread_get_cap(l4x_cpu_threads[cpu]),
	                          l4x_ipi_irq << 2);
#else
	t = l4_rcv_ep_bind_thread(c,
	                          l4lx_thread_get_cap(l4x_cpu_ipi_threads[cpu]),
	                          0);
#endif
	if (l4_error(t))
		l4x_exit_l4linux_msg("Failed to attach IPI IRQ%d\n", cpu);

	// now commit and make cap visible to child, it can run now
	mb();
	l4x_cpu_ipi_irqs[cpu] = c;
}

void l4x_cpu_ipi_stop(unsigned cpu)
{
	long e;

	e = L4XV_FN_e(l4_irq_detach(l4x_cpu_ipi_irqs[cpu]));
	if (e) {
		l4x_printf("Failed to detach for IPI IRQ%d\n", cpu);
		return;
	}

	e = L4XV_FN_e(l4_task_delete_obj(L4RE_THIS_TASK_CAP,
	              l4x_cpu_ipi_irqs[cpu]));
	if (e) {
		l4x_printf("Failed to unmap IPI IRQ%d\n", cpu);
		return;
	}

	l4x_cap_free(l4x_cpu_ipi_irqs[cpu]);
	l4x_cpu_ipi_irqs[cpu] = L4_INVALID_CAP;

#ifndef CONFIG_L4_VCPU
	l4lx_thread_shutdown(l4x_cpu_ipi_threads[cpu], 1, NULL, 1);
#endif
}
#endif /* SMP */


static void init_irq(unsigned int irq, struct irq_chip *chip,
                     l4_cap_idx_t icu)
{
	struct l4x_irq_desc_private *p;

	irq_set_chip_and_handler(irq, chip, handle_level_irq);
	irq_clear_status_flags(irq, IRQ_NOREQUEST);
	p = l4x_alloc_irq_desc_data(irq, icu, irq);
	irq_set_chip_data(irq, p);
}

static int domain_map_icu_chip(struct irq_domain *d, unsigned int irq,
                               irq_hw_number_t hw)
{
	init_irq(irq, &l4x_irq_icu_chip, (l4_cap_idx_t)d->host_data);
	return 0;
}

static struct irq_domain_ops l4x_irq_domain_icu_ops = {
	.map = domain_map_icu_chip,
};

int l4x_init_icu_irqs(struct device_node *devnode, l4_cap_idx_t icu)
{
	l4_icu_info_t icu_info;
	int r;
	struct irq_domain *domain;

	r = l4_error(l4_icu_info(icu, &icu_info));
	if (r) {
		pr_err("l4x: Cannot query L4ICU: %d\n", r);
		return -ENOENT;
	}

	domain = irq_domain_add_linear(devnode, icu_info.nr_irqs,
	                               &l4x_irq_domain_icu_ops,
	                               (void *)icu);
	if (!domain) {
		pr_err("l4icu: ICU domain creation failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void irqchip_dummy_op(struct irq_data *d)
{
}

static void irqchip_ipi_send_mask(struct irq_data *d,
                                  const struct cpumask *mask)
{
	int cpu;

	for_each_cpu(cpu, mask) {
		l4x_cpu_ipi_enqueue_vector(cpu, d->hwirq);
		l4x_cpu_ipi_trigger(cpu);
	}
}

static struct irq_chip l4x_irqchip_ipi = {
	.name           = "IPI",
	.irq_mask       = irqchip_dummy_op,
	.irq_unmask     = irqchip_dummy_op,
	.irq_eoi        = irqchip_dummy_op,
	.ipi_send_mask  = irqchip_ipi_send_mask,
};

static int ipi_alloc(struct irq_domain *d,
                     unsigned int virq,
                     unsigned int nr_irqs, void *args)
{
	int i;

	for (i = 0; i < nr_irqs; i++) {
		irq_set_percpu_devid(virq + i);
		irq_domain_set_info(d, virq + i, i, &l4x_irqchip_ipi,
		                    d->host_data,
		                    handle_percpu_devid_irq,
		                    NULL, NULL);
	}

	return 0;
}

static void ipi_free(struct irq_domain *d,
                     unsigned int virq,
                     unsigned int nr_irqs)
{
	/* Not freeing IPIs */
}


static const struct irq_domain_ops ipi_domain_ops = {
        .alloc  = ipi_alloc,
        .free   = ipi_free,
};

void l4x_cpu_ipi_trigger(unsigned cpu)
{
	long e = L4XV_FN_e(l4_irq_trigger(l4x_cpu_ipi_irqs[cpu]));
	if (unlikely(e != L4_PROTO_IRQ))
		l4x_printf("Trigger of IPI%d failed\n", cpu);
}

void l4x_cpu_ipi_enqueue_vector(unsigned cpu, unsigned vector)
{
	l4_umword_t old, ret;
	do {
		old = l4x_cpu_ipi_vector_mask[cpu];
		ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
		                     old, old | (1 << vector));
	} while (!ret);
}

#if defined(CONFIG_X86) && defined(CONFIG_SMP)
void l4x_x86_smp_process_ipi(int vector, struct pt_regs *regs)
{
	switch (vector | L4X_SMP_IPI_VECTOR_BASE) {
		case RESCHEDULE_VECTOR:
			sysvec_reschedule_ipi(regs);
			break;
		case CALL_FUNCTION_VECTOR:
			sysvec_call_function(regs);
			break;
		case CALL_FUNCTION_SINGLE_VECTOR:
			sysvec_call_function_single(regs);
			break;
		case REBOOT_VECTOR:
			irq_enter();
			stop_this_cpu(NULL);
			irq_exit();
			break;
		case IRQ_WORK_VECTOR:
			sysvec_irq_work(regs);
			break;
		default:
			pr_warn("Spurious IPI on CPU%d: %x\n",
			        smp_processor_id(), vector);
			break;
	}
}

void l4x_x86_vcpu_handle_ipi(struct pt_regs *regs)
{
	unsigned cpu = smp_processor_id();

	while (1) {
		l4_umword_t ret, mask;
		unsigned vector;
		do {
			mask = l4x_cpu_ipi_vector_mask[cpu];
			ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
			                     mask, 0);
		} while (unlikely(!ret));

		if (!mask)
			break;

		while (mask) {
			vector = ffs(mask) - 1;
			l4x_x86_smp_process_ipi(vector, regs);
			mask &= ~(1 << vector);
		}
	}
}
#endif /* X86 && SMP */

#if defined(CONFIG_SMP) && (defined(CONFIG_ARM) || defined(CONFIG_ARM64))
static void l4x_handle_ipi(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned cpu = smp_processor_id();

	chained_irq_enter(chip, desc);

	while (1) {
		l4_umword_t ret, mask;
		unsigned vector;
		do {
			mask = l4x_cpu_ipi_vector_mask[cpu];
			ret = l4util_cmpxchg(&l4x_cpu_ipi_vector_mask[cpu],
			                     mask, 0);
		} while (unlikely(!ret));

		if (!mask)
			break;

		while (mask) {
			vector = ffs(mask) - 1;
			generic_handle_irq(irq_find_mapping(ipi_domain,
			                                    vector));
			mask &= ~(1 << vector);
		}
	}

	chained_irq_exit(chip, desc);
}
#endif /* ARM-both && SMP */

static __init void l4x_init_irq_smp(void)
{
#ifdef CONFIG_SMP
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	struct l4x_irq_desc_private *p;

	ipi_domain = irq_domain_create_linear(NULL,
	                                      IPI_FUNCS, &ipi_domain_ops, NULL);
	if (WARN_ON(!ipi_domain))
		return;

	ipi_domain->flags |= IRQ_DOMAIN_FLAG_IPI_SINGLE;
	irq_domain_update_bus_token(ipi_domain, DOMAIN_BUS_IPI);

	l4x_ipi_irq = irq_domain_alloc_irqs(ipi_domain, IPI_FUNCS,
	                                    NUMA_NO_NODE, NULL);
	if (WARN_ON(l4x_ipi_irq <= 0))
		return;

	set_smp_ipi_range(l4x_ipi_irq, IPI_FUNCS);

	irq_set_percpu_devid(l4x_ipi_irq);

	p = l4x_alloc_irq_desc_data(l4x_ipi_irq, L4_INVALID_CAP, l4x_ipi_irq);
	WARN_ON(!p);
	irq_set_chip_data(l4x_ipi_irq, p);

	irq_set_chained_handler(l4x_ipi_irq, l4x_handle_ipi);
#endif /* ARM-both */
#ifdef CONFIG_X86
	/* On x86, we just reserve the IRQ number here (is there a better
	 * way of doing it?) and then call the handler directly as done
	 * bare-metal */
	l4x_ipi_irq = irq_create_direct_mapping(l4x_irq_domain);
	WARN_ON(l4x_ipi_irq <= 0);
#endif /* X86 */

        l4x_cpu_ipi_setup(0);
#endif /* SMP */
}

void __init l4x_init_irq(void)
{
	struct fwnode_handle *fn;

        fn = irq_domain_alloc_named_fwnode("L4x");

	/* This is irq_domain_add_nomap */
	l4x_irq_domain = __irq_domain_add(fn, 0, 1024, 1024,
	                                  &l4x_irq_domain_ops, NULL);
	BUG_ON(!l4x_irq_domain);
	irq_set_default_host(l4x_irq_domain);

#ifdef CONFIG_X86_LOCAL_APIC
	x86_vector_domain = l4x_irq_domain;
#endif

	l4x_init_irq_smp();

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	/* Try without device tree */
	if (!l4x_of_found_l4icu)
		l4x_init_icu_irqs(NULL, l4io_request_icu());
#endif /* ARM-both */
}

#ifdef CONFIG_OF
static int domain_xlate_gic(struct irq_domain *d,
                            struct device_node *controller,
                            const u32 *intspec, unsigned int intsize,
                            unsigned long *out_hwirq,
                            unsigned int *out_type)
{
	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;

	if (intsize < 3)
		return -EINVAL;

	switch (intspec[0]) {
		case 0: /* SPI */
			*out_hwirq = intspec[1] + 32;
			break;
		case 1: /* PPI */
			*out_hwirq = intspec[1] + 16;
			break;
		default:
			return -EINVAL;
	};

	*out_type  = intspec[2] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int domain_xlate_single(struct irq_domain *d,
                               struct device_node *controller,
                               const u32 *intspec, unsigned int intsize,
                               unsigned long *out_hwirq,
                               unsigned int *out_type)
{
	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;

	if (intsize < 1)
		return -EINVAL;

	*out_hwirq = intspec[0];
	*out_type  = IRQ_TYPE_LEVEL_HIGH;

	return 0;
}

static int __init irq_l4icu_init(struct device_node *node,
                                 struct device_node *parent)
{
	l4_cap_idx_t icu = L4_INVALID_CAP;
	const char *capname;
	int r;
	enum irq_type_t {
		TYPE_NONE,
		TYPE_GIC_TRIPLE,
		TYPE_SINGLE,
	};
	enum irq_type_t irq_type = TYPE_NONE;
	const char *icutypestr;
	int num_cells_needed;
	const __be32 *tmp;

	if (!of_property_read_string(node, "l4icu-type", &icutypestr)) {
		if (!strcmp(icutypestr, "gic"))
			irq_type = TYPE_GIC_TRIPLE;
		else if (!strcmp(icutypestr, "single"))
			irq_type = TYPE_SINGLE;
	}

	switch (irq_type) {
	case TYPE_GIC_TRIPLE:
		l4x_irq_domain_icu_ops.xlate = domain_xlate_gic;
		num_cells_needed = 3;
		break;
	case TYPE_SINGLE:
		l4x_irq_domain_icu_ops.xlate = domain_xlate_single;
		num_cells_needed = 1;
		break;
	case TYPE_NONE:
		pr_err("l4,icu: 'l4icu-type' must be specified in DT.\n"
		       "        Possible values: gic, single.\n");
		BUG();
	};

	/* This is just a sanity check */
	tmp = of_get_property(node, "#interrupt-cells", NULL);
	if (tmp) {
		u32 n = be32_to_cpu(*tmp);

		if (n != num_cells_needed)
			pr_warn("l4,icu: Requested type wants %d "
			        "interrupt-cells but %d given\n",
			        num_cells_needed, n);
	} else
		pr_warn("l4,icu: DT entry has no #interrupt-cells property.\n");


	if (!of_property_read_string(node, "cap", &capname))
		if (l4x_re_resolve_name(capname, &icu)) {
			pr_err("l4x: ICU cap '%s' not found.\n",
			        capname);
			icu = L4_INVALID_CAP;
		}

	if (l4_is_invalid_cap(icu))
		icu = l4io_request_icu();

	if (l4_is_invalid_cap(icu)) {
		pr_err("l4x: Invalid L4ICU cap.\n");
		return -ENOENT;
	}

	r = l4x_init_icu_irqs(node, icu);
	if (r)
		return r;

	l4x_of_found_l4icu = 1;

	return 0;
}

IRQCHIP_DECLARE(l4lx_l4icu, "l4,icu", irq_l4icu_init);
#endif /* OF */

#if defined(CONFIG_X86) && defined(CONFIG_SMP)
void l4x_send_IPI_mask_bitmask(unsigned long mask, int vector)
{
	int cpu;

	if (unlikely(vector >= (sizeof(unsigned long) * 8)))
		l4x_printf("BIG vector %d (caller %lx)\n", vector, _RET_IP_);

	BUILD_BUG_ON(NR_CPUS > sizeof(l4x_cpu_ipi_vector_mask[cpu]) * 8);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (!(mask & (1UL << cpu)))
			continue;

		l4x_cpu_ipi_enqueue_vector(cpu, vector);
		l4x_cpu_ipi_trigger(cpu);
	}
}
#endif /* X86 && SMP */
