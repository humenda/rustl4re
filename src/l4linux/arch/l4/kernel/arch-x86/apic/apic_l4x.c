#include <asm/ipi.h>
#include <asm/apic.h>
#include <asm/generic/smp_ipi.h>

/* Similar to default_send_IPI_mask_logical() */
void l4x_send_IPI_mask(const struct cpumask *cpumask, int vector)
{
	unsigned long mask = cpumask_bits(cpumask)[0];
	unsigned long flags;

	if (!mask)
		return;

	local_irq_save(flags);
	WARN_ON(mask & ~cpumask_bits(cpu_online_mask)[0]);
	l4x_send_IPI_mask_bitmask(mask, vector & L4X_SMP_IPI_VECTOR_MASK);
	local_irq_restore(flags);
}

#ifdef CONFIG_SMP
/* Similar to default_send_IPI_allbutself() */
static void l4x_send_IPI_allbutself(int vector)
{
	/*
	 * if there are no other CPUs in the system then we get an APIC send
	 * error if we try to broadcast, thus avoid sending IPIs in this case.
	 */
	if (!(num_online_cpus() > 1))
		return;

	__l4x_send_IPI_shortcut(APIC_DEST_ALLBUT, vector);
}

/* Similar to default_send_IPI_all() */
static void l4x_send_IPI_all(int vector)
{
        __l4x_send_IPI_shortcut(APIC_DEST_ALLINC, vector);
}

static void l4x_send_IPI_self(int vector)
{
	__l4x_send_IPI_shortcut(APIC_DEST_SELF, vector);
}
#endif

static int apicid_phys_pkg_id(int initial_apic_id, int index_msb)
{
	return hard_smp_processor_id() >> index_msb;
}

static void noop_apic_wait_icr_idle(void) { }
static u32 noop_safe_apic_wait_icr_idle(void)
{
	return 0;
}

static struct apic l4x_apic __ro_after_init =  {
	.name				= "l4x-apic",
	.apic_id_valid			= default_apic_id_valid,

	.cpu_present_to_apicid		= default_cpu_present_to_apicid,
	.check_phys_apicid_present	= default_check_phys_apicid_present,
	.phys_pkg_id			= apicid_phys_pkg_id,

	.calc_dest_apicid		= apic_default_calc_apicid,

	.wait_icr_idle			= noop_apic_wait_icr_idle,
	.safe_wait_icr_idle		= noop_safe_apic_wait_icr_idle,

#ifdef CONFIG_SMP
	.send_IPI			= default_send_IPI_single,
	.send_IPI_mask			= l4x_send_IPI_mask,
	.send_IPI_mask_allbutself	= default_send_IPI_mask_allbutself_phys,
	.send_IPI_allbutself		= l4x_send_IPI_allbutself,
	.send_IPI_all			= l4x_send_IPI_all,
	.send_IPI_self			= l4x_send_IPI_self,
#endif
};



struct apic __read_mostly *apic = &l4x_apic;
EXPORT_SYMBOL_GPL(apic);
