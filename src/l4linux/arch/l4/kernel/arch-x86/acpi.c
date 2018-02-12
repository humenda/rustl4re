/*
 * Basic ACPI setup for booting
 */

#include <linux/init.h>
#include <linux/acpi.h>

int acpi_ht __initdata;
int acpi_pci_disabled __initdata = 1;
int acpi_noirq __initdata = 1;
int acpi_disabled = 1;

int acpi_lapic;
int acpi_ioapic;
int acpi_strict;

enum acpi_irq_model_id acpi_irq_model = ACPI_IRQ_MODEL_PIC;

u8 acpi_sci_flags __initdata;
int acpi_sci_override_gsi __initdata;
int acpi_skip_timer_override __initdata;
int acpi_use_timer_override __initdata;


int __init acpi_boot_init(void)
{
	return 1; /* Failure */
}

char *__acpi_map_table(unsigned long phys, unsigned long size)
{
	return 0;
}

unsigned long __init acpi_find_rsdp(void)
{
	return 0;
}

int __init acpi_mps_check(void)
{
	return 1; /* Failure */
}

int __init acpi_boot_table_init(void)
{
	return 1; /* Failure */
}

int acpi_register_gsi(u32 gsi, int triggering, int polarity)
{
	return -1;
}

int acpi_gsi_to_irq(u32 gsi, unsigned int *irq)
{
	*irq = gsi;
	return 0;
}

int __acpi_acquire_global_lock(unsigned int *lock)
{
	return -1;
}

int __acpi_release_global_lock(unsigned int *lock)
{
	return -1;
}

void __init acpi_pic_sci_set_trigger(unsigned int irq, u16 trigger)
{
}

int __init early_acpi_boot_init(void)
{
	return 1;
}

/* processor.c */

#include <acpi/processor.h>
void arch_acpi_processor_init_pdc(struct acpi_processor *pr)
{
}

/* cstate.c */
int acpi_processor_ffh_cstate_probe(unsigned int cpu,
		                struct acpi_processor_cx *cx, struct
				acpi_power_register *reg)
{
	return -1;
}

void acpi_processor_power_init_bm_check(struct acpi_processor_flags *flags,
		                                        unsigned int cpu)
{
}

void acpi_processor_ffh_cstate_enter(struct acpi_processor_cx *cx)
{
}
