#pragma once

#ifdef CONFIG_PCI_MSI

void l4x_compose_msi_msg(struct irq_data *data, struct msi_msg *msg);
void l4x_teardown_msi_irq(unsigned int irq);
int l4x_setup_msi_irq_init(unsigned irq, struct irq_chip *msi_chip);

#endif
