#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/irqchip.h>

#include <asm/mach-types.h>
#include <asm/delay.h>
#include <asm/outercache.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

#include <asm/setup.h>

#include <asm/l4lxapi/irq.h>
#include <asm/api/config.h>
#include <asm/generic/irq.h>
#include <asm/generic/setup.h>
#include <asm/generic/timer.h>
#include <asm/generic/util.h>

#include <l4/sys/icu.h>
#include <l4/sys/cache.h>
#include <l4/io/io.h>

enum { L4X_PLATFORM_INIT_GENERIC = 1 };

static void __init l4x_mach_map_io(void)
{
}

static void __init l4x_mach_fixup(struct tag *tags, char **cmdline)
{
	*cmdline = boot_command_line;
}

#ifdef CONFIG_DEBUG_LL
#include <l4/sys/kdebug.h>
void printascii(const char *buf)
{
	outstring(buf);
}
#endif

static void init_irq(unsigned int irq, struct irq_chip *chip,
                     l4_cap_idx_t icu)
{
	struct l4x_irq_desc_private *p;

	irq_set_chip_and_handler(irq, chip, handle_level_irq);
	irq_clear_status_flags(irq, IRQ_NOREQUEST);
	p = l4x_alloc_irq_desc_data(irq, icu, irq);
	irq_set_chip_data(irq, p);
}

static int l4x_of_found_l4icu;

static int domain_map_icu_chip(struct irq_domain *d, unsigned int irq,
                               irq_hw_number_t hw)
{
	init_irq(irq, &l4x_irq_icu_chip, (l4_cap_idx_t)d->host_data);
	return 0;
}

static struct irq_domain_ops l4x_irq_domain_icu_ops = {
	.map   = domain_map_icu_chip,
};

static struct irq_domain_ops l4x_irq_domain_plain_ops;

static int init_icu_irqs(struct device_node *devnode, l4_cap_idx_t icu)
{
	l4_icu_info_t icu_info;
	int r, irq_base;
	struct irq_domain *domain;

	r = l4_error(l4_icu_info(icu, &icu_info));
	if (r) {
		pr_err("l4x: Cannot query L4ICU: %d\n", r);
		return -ENOENT;
	}

	BUG_ON(L4X_IRQS_V_DYN_BASE < icu_info.nr_irqs);

	irq_base = irq_alloc_descs(0, 0, icu_info.nr_irqs, numa_node_id());
	if (IS_ERR_VALUE(irq_base)) {
		pr_err("l4x-irq-init: Failed to alloc IRQ descs\n");
		return -ENOMEM;
	}

	// could very well use irq_domain_add_nomap()
	domain = irq_domain_add_legacy(devnode, icu_info.nr_irqs,
	                               irq_base, 0, &l4x_irq_domain_icu_ops,
	                               (void *)icu);
	if (!domain) {
		pr_err("l4icu: ICU domain creation failed\n");
		irq_free_descs(irq_base, icu_info.nr_irqs);
		return -ENOMEM;
	}

	return 0;
}

static int init_dyn_irqs(void)
{
	struct irq_domain *domain;

	domain = irq_domain_add_legacy(NULL, L4X_NR_IRQS_V_DYN,
	                               L4X_IRQS_V_DYN_BASE,
	                               L4X_IRQS_V_DYN_BASE,
				       &l4x_irq_domain_plain_ops, NULL);
	if (!domain) {
		pr_err("l4icu: Plain domain creation failed\n");
		return -ENOMEM;
	}

	return 0;
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


int __init irq_l4icu_init(struct device_node *node,
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

	r = init_icu_irqs(node, icu);
	if (r)
		return r;

	l4x_of_found_l4icu = 1;

	return 0;
}

IRQCHIP_DECLARE(l4lx_l4icu, "l4,icu", irq_l4icu_init);
#endif /* OF */

static void __init l4x_mach_init_irq(void)
{
	irqchip_init();

	/* Call our generic IRQ handling code */
	l4lx_irq_init();

	if (!l4x_of_found_l4icu)
		init_icu_irqs(NULL, l4io_request_icu());

	init_dyn_irqs();
}

#ifdef CONFIG_L4_CLK_NOOP
int clk_enable(struct clk *clk)
{
	printk("%s %d\n", __func__, __LINE__);
        return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	printk("%s %d\n", __func__, __LINE__);
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
        return 1000;
}
EXPORT_SYMBOL(clk_get_rate);

#endif

int dma_needs_bounce(struct device *d, dma_addr_t a, size_t s)
{
	return 1;
}


#ifdef CONFIG_OUTER_CACHE
int __init l2x0_of_init(u32 aux_val, u32 aux_mask)
{
	return 0;
}

static void l4x_outer_cache_inv_range(unsigned long start, unsigned long end)
{
	l4_cache_l2_inv((unsigned long)phys_to_virt(start),
	                (unsigned long)phys_to_virt(end));
}

static void l4x_outer_cache_clean_range(unsigned long start, unsigned long end)
{
	l4_cache_l2_clean((unsigned long)phys_to_virt(start),
	                  (unsigned long)phys_to_virt(end));
}

static void l4x_outer_cache_flush_range(unsigned long start, unsigned long end)
{
	l4_cache_l2_flush((unsigned long)phys_to_virt(start),
	                  (unsigned long)phys_to_virt(end));
}

static void l4x_outer_cache_flush_all(void)
{
	printk("%s called by %p\n", __func__, __builtin_return_address(0));
}

static void l4x_outer_cache_disable(void)
{
	printk("%s called by %p\n", __func__, __builtin_return_address(0));
}

static void __init l4x_setup_outer_cache(void)
{
	outer_cache.inv_range   = l4x_outer_cache_inv_range;
	outer_cache.clean_range = l4x_outer_cache_clean_range;
	outer_cache.flush_range = l4x_outer_cache_flush_range;
	outer_cache.flush_all   = l4x_outer_cache_flush_all;
	outer_cache.disable     = l4x_outer_cache_disable;
}
#endif

static struct {
	struct tag_header hdr1;
	struct tag_core   core;
	struct tag_header hdr2;
	struct tag_mem32  mem;
	struct tag_header hdr3;
} l4x_atag __initdata = {
	{ tag_size(tag_core), ATAG_CORE },
	{ 1, PAGE_SIZE, 0xff },
	{ tag_size(tag_mem32), ATAG_MEM },
	{ 0 },
	{ 0, ATAG_NONE }
};

static void __init l4x_mach_init(void)
{
	l4x_atag.mem.start = PAGE_SIZE;
	l4x_atag.mem.size  = 0xbf000000 - PAGE_SIZE;

#ifdef CONFIG_OUTER_CACHE
	l4x_setup_outer_cache();
#endif

#ifdef CONFIG_OF
	if (L4X_PLATFORM_INIT_GENERIC)
		of_platform_populate(NULL, of_default_bus_match_table,
		                     NULL, NULL);
#endif
}

static inline void l4x_stop(enum reboot_mode mode, const char *cmd)
{
	local_irq_disable();
	l4x_platform_shutdown(1U);
}

static struct delay_timer arch_delay_timer;

static unsigned long read_cntvct(void)
{
	unsigned long long v;
	asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (v));
	return v;
}

static unsigned long read_cntfrq(void)
{
	unsigned long v;
	asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (v));
	return v;
}

static void setup_delay_counter(void)
{
	arch_delay_timer.read_current_timer = read_cntvct;
	arch_delay_timer.freq = read_cntfrq();
	BUG_ON(arch_delay_timer.freq == 0);
	register_current_timer_delay(&arch_delay_timer);
}

static void __init l4x_arm_timer_init(void)
{
	l4x_timer_init();

#ifdef CONFIG_COMMON_CLK
	of_clk_init(NULL);
#endif
	timer_probe();

	if (0)
		setup_delay_counter();
}

extern const struct smp_operations l4x_smp_ops;

MACHINE_START(L4, "L4")
	.atag_offset    = (unsigned long)&l4x_atag - PAGE_OFFSET,
	.smp		= smp_ops(l4x_smp_ops),
	.fixup		= l4x_mach_fixup,
	.map_io		= l4x_mach_map_io,
	.init_irq	= l4x_mach_init_irq,
	.init_time	= l4x_arm_timer_init,
	.init_machine	= l4x_mach_init,
	.restart	= l4x_stop,
MACHINE_END

static char const *l4x_dt_compat[] __initdata = {
	"L4Linux",
	NULL
};

DT_MACHINE_START(L4_DT, "L4Linux (DT)")
	.smp		= smp_ops(l4x_smp_ops),
	.map_io		= l4x_mach_map_io,
	.init_machine	= l4x_mach_init,
	.init_late	= NULL,
	.init_irq	= l4x_mach_init_irq,
	.init_time	= l4x_arm_timer_init,
	.dt_compat	= l4x_dt_compat,
	.restart	= l4x_stop,
MACHINE_END

/*
 * We only have one machine description for now, so keep lookup_machine_type
 * simple.
 */
const struct machine_desc *lookup_machine_type(unsigned int x)
{
	return &__mach_desc_L4;
}

#ifdef CONFIG_SMP
#include <asm/generic/smp_ipi.h>

void l4x_raise_softirq(const struct cpumask *mask, unsigned ipi)
{
	int cpu;

	for_each_cpu(cpu, mask) {
		l4x_cpu_ipi_enqueue_vector(cpu, ipi);
		l4x_cpu_ipi_trigger(cpu);
	}
}
#endif
