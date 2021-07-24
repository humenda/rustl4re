#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/irqchip.h>

#include <asm/mach-types.h>
#include <asm/delay.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

#include <asm/setup.h>

#include <asm/l4lxapi/irq.h>
#include <asm/api/config.h>
#include <asm/generic/irq.h>
#include <asm/generic/setup.h>
#include <asm/generic/timer.h>

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

static void __init l4x_mach_init_irq(void)
{
	irqchip_init();

	/* Call our generic IRQ handling code */
	l4lx_irq_init();

	l4x_init_irq();
}

int dma_needs_bounce(struct device *d, dma_addr_t a, size_t s)
{
	return 1;
}

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
