#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/irqdomain.h>

#include <asm/uaccess.h>
#include <asm/msidef.h>
#include <asm/irqdomain.h>
#include <asm/irq_remapping.h>

#include <asm/l4lxapi/irq.h>

#include <asm/generic/irq.h>
#include <asm/generic/task.h>
#include <asm/generic/smp.h>
#include <asm/generic/stack_id.h>
#include <asm/l4x/irq_msi.h>

#include <l4/sys/icu.h>
#include <l4/io/io.h>

static l4_icu_info_t l4x_icu_info;

#ifdef CONFIG_PCI_MSI
static unsigned long *l4x_msi_alloc_bitmap;
static DEFINE_SPINLOCK(l4x_msi_alloc_lock);
#endif

void __init l4x_init_IRQ(void)
{
	l4_cap_idx_t icu = l4io_request_icu();
	struct l4x_irq_desc_private *p;
	int i;

	int ret = L4XV_FN_e(l4_icu_info(icu, &l4x_icu_info));
	if (ret < 0) {
		pr_err("l4x: could not get ICU info: %d\n", ret);
	} else if (l4x_icu_info.features & L4_ICU_FLAG_MSI) {
		pr_info("l4x-msi: ICU supports %u MSI vectors\n",
		        l4x_icu_info.nr_msis);
#ifdef CONFIG_PCI_MSI
		l4x_msi_alloc_bitmap = kzalloc(BITS_TO_LONGS(l4x_icu_info.nr_msis)
		                               * sizeof(unsigned long), GFP_KERNEL);
		if (!l4x_msi_alloc_bitmap)
			pr_err("l4x-msi: could not allocate MSI allocator bitmap\n");
#endif
	}

	l4lx_irq_init();

	for (i = 0; i < L4X_IRQS_V_DYN_BASE; i++) {
		p = l4x_alloc_irq_desc_data(i, icu, i);
		irq_set_chip_data(i, p);
		irq_set_chip_and_handler(i, &l4x_irq_icu_chip,
		                         handle_edge_eoi_irq);
	}

	/* from native_init_IRQ() */
#ifdef CONFIG_X86_32
	irq_ctx_init(smp_processor_id());
#endif
}

#ifdef CONFIG_PCI_MSI
void l4x_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);
	struct irq_cfg *cfg = irqd_cfg(data);
	int ret;
	l4_icu_msi_info_t msiinfo;
	struct msi_desc *msidesc = irq_data_get_msi_desc(data);
	struct pci_dev *pcidev = msi_desc_to_pci_dev(msidesc);
	unsigned src_id = 0x40000 | pcidev->bus->number << 8 | pcidev->devfn;

	ret = L4XV_FN_e(l4_icu_msi_info(p->icu, data->hwirq, src_id, &msiinfo));
	if (ret < 0) {
		pr_err("l4x-msi: l4_icu_msi_info failed for ICU IRQ: %lx\n",
		       data->hwirq);
		msg->data = 1 << 31;
		return;
	}

	if (msiinfo.msi_addr & 0x10) {
		/* Remappable MSI */
		msg->address_lo = msiinfo.msi_addr & 0xffffffff;
		msg->address_hi = msiinfo.msi_addr >> 32;
		msg->data = msiinfo.msi_data;

		return;
	}

	msg->address_lo =   MSI_ADDR_BASE_LO
	                  | MSI_ADDR_DEST_MODE_PHYSICAL
	                  | MSI_ADDR_REDIRECTION_CPU
			  | MSI_ADDR_DEST_ID(cfg->dest_apicid);
	msg->address_hi = MSI_ADDR_BASE_HI;
	// if x2apic: add MSI_ADDR_EXT_DEST_ID(cfg->dest_apicid);
	msg->data = MSI_DATA_TRIGGER_EDGE |
		    MSI_DATA_LEVEL_ASSERT |
		    MSI_DATA_DELIVERY_FIXED |
	            MSI_DATA_VECTOR(msiinfo.msi_data);
}

static void l4x_free_msi(unsigned icu_irq)
{
	spin_lock(&l4x_msi_alloc_lock);
	clear_bit(icu_irq, l4x_msi_alloc_bitmap);
	spin_unlock(&l4x_msi_alloc_lock);
}

static void l4x_deinit_msi_irq(unsigned irq)
{
	struct irq_data *data = irq_get_irq_data(irq);

	l4x_irq_deinit(data);
	l4x_free_msi(data->hwirq & 0xffff);
}

void l4x_teardown_msi_irq(unsigned int irq)
{
	struct irq_data *data = irq_get_irq_data(irq);
	struct l4x_irq_desc_private *p = irq_data_get_irq_chip_data(data);

	l4x_deinit_msi_irq(irq);
	l4x_free_irq_dest_data(p);
}

int l4x_setup_msi_irq_init(unsigned irq, struct irq_chip *msi_chip)
{
	l4_cap_idx_t icu = l4io_request_icu();
	l4_cap_idx_t irqcap;
	int ret;
	int icu_irq;
	struct irq_data *data = irq_get_irq_data(irq);

	if (!l4x_msi_alloc_bitmap)
		return -ENODEV;

	spin_lock(&l4x_msi_alloc_lock);
	icu_irq = find_first_zero_bit(l4x_msi_alloc_bitmap,
	                              l4x_icu_info.nr_msis);
	if (icu_irq >= l4x_icu_info.nr_msis) {
		spin_unlock(&l4x_msi_alloc_lock);
		return -ENODEV;
	}
	set_bit(icu_irq, l4x_msi_alloc_bitmap);
	spin_unlock(&l4x_msi_alloc_lock);

	irqcap = l4x_irq_init(icu, icu_irq | L4_ICU_FLAG_MSI,
	                      L4_IRQ_F_NEG_EDGE, "l4-msi");
	if (l4_is_invalid_cap(irqcap)) {
		l4x_free_msi(icu_irq);
		return -ENODEV;
	}

	ret = l4x_init_dyn_irq(irq, irqcap, icu,
	                       icu_irq | L4_ICU_FLAG_MSI, msi_chip);
	if (ret < 0) {
		l4x_free_msi(icu_irq);
		l4x_irq_deinit(data);
	}

	return ret;
}

static int l4x_do_msi_set_affinity(struct irq_data *data,
                                   unsigned logical_dest)
{
#ifndef CONFIG_L4
	struct irq_cfg *cfg = data->chip_data;
#endif
	struct msi_msg msg;
	l4_uint32_t dest = cpu_data(logical_dest).initial_apicid;

	__get_cached_msi_msg(irq_data_get_msi_desc(data), &msg);

#ifndef CONFIG_L4
	msg.data &= ~MSI_DATA_VECTOR_MASK;
	msg.data |= MSI_DATA_VECTOR(cfg->vector);
#endif
	msg.address_lo &= ~MSI_ADDR_DEST_ID_MASK;
	msg.address_lo |= MSI_ADDR_DEST_ID(dest);

	__write_msi_msg(irq_data_get_msi_desc(data), &msg);

	return 0;
}


int l4lx_irq_msi_set_affinity(struct irq_data *data,
                              const struct cpumask *dest, bool force)
{
	return do_l4lx_irq_set_affinity(data, dest, force,
	                                l4x_do_msi_set_affinity);
}

void l4lx_irq_msi_ack(struct irq_data *data)
{
	irq_move_irq(data);
}
#endif /* PCI_MSI */

int arch_setup_hwirq(unsigned int irq, int node)
{
	return 0;
}

void arch_teardown_hwirq(unsigned int irq)
{
}
