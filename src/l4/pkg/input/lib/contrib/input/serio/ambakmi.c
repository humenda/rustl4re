/*
 *  linux/drivers/input/serio/ambakmi.c
 *
 *  Copyright (C) 2000-2003 Deep Blue Solutions Ltd.
 *  Copyright (C) 2002 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <asm/io.h>
#include <asm/hardware/amba_kmi.h>

#include <l4/io/io.h>

#define AMBA_NR_IRQS	2

struct amba_device {
	struct device		dev;
	struct resource		res;
	u64			dma_mask;
	unsigned int		periphid;
	unsigned int		irq[AMBA_NR_IRQS];
};

static struct amba_device dev_kmi_k;
static struct amba_device dev_kmi_m;

enum {
  // DATA
  DATA_RESET          = 0xff,
  DATA_RESET_RESPONSE = 0xaa,

  // CLKDIV
  CLKDIV_DIVISOR = 2,   /* 8MHz = 24MHz / (1 + CLKDIV_DIVISOR) */
};


#define KMI_BASE	(kmi->base)

struct amba_kmi_port {
	struct serio		*io;
	struct clk		*clk;
	void  *base;
	unsigned int		irq;
	unsigned int		divisor;
	unsigned int		open;
};

static irqreturn_t amba_kmi_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct amba_kmi_port *kmi = dev_id;
	unsigned int status = readb(KMIIR);
	int handled = IRQ_NONE;

	while (status & KMIIR_RXINTR) {
		serio_interrupt(kmi->io, readb(KMIDATA), 0, regs);
		status = readb(KMIIR);
		handled = IRQ_HANDLED;
	}

	return handled;
}

static int amba_kmi_write(struct serio *io, unsigned char val)
{
	struct amba_kmi_port *kmi = io->port_data;
	unsigned int timeleft = 10000; /* timeout in 100ms */

	while ((readb(KMISTAT) & KMISTAT_TXEMPTY) == 0 && timeleft--)
		udelay(10);

	if (timeleft)
		writeb(val, KMIDATA);

	return timeleft ? 0 : SERIO_TIMEOUT;
}

static int amba_kmi_open(struct serio *io)
{
	struct amba_kmi_port *kmi = io->port_data;
	unsigned int divisor;
	int ret;

	divisor = CLKDIV_DIVISOR;
	writeb(divisor, KMICLKDIV);
	writeb(KMICR_EN, KMICR);

	ret = request_irq(kmi->irq, amba_kmi_int, 0, "kmi-pl050", kmi);
	if (ret) {
		printk(KERN_ERR "kmi: failed to claim IRQ%d\n", kmi->irq);
		writeb(0, KMICR);
		return ret;
	}

	writeb(KMICR_EN | KMICR_RXINTREN, KMICR);
	return 0;
}

static void amba_kmi_close(struct serio *io)
{
	struct amba_kmi_port *kmi = io->port_data;

	writeb(0, KMICR);

	free_irq(kmi->irq, kmi);
}

static int amba_kmi_probe(struct amba_device *dev, void *id)
{
	struct amba_kmi_port *kmi;
	struct serio *io;
	int ret;

	kmi = kmalloc(sizeof(struct amba_kmi_port), GFP_KERNEL);
	io = kmalloc(sizeof(struct serio), GFP_KERNEL);
	if (!kmi || !io) {
		ret = -ENOMEM;
		goto out;
	}

	memset(kmi, 0, sizeof(struct amba_kmi_port));
	memset(io, 0, sizeof(struct serio));

	io->type	= SERIO_8042;
	io->write	= amba_kmi_write;
	io->open	= amba_kmi_open;
	io->close	= amba_kmi_close;
	strlcpy(io->name, dev->dev.bus_id, sizeof(io->name));
	strlcpy(io->phys, dev->dev.bus_id, sizeof(io->phys));
	io->port_data   = kmi;
	//io->dev.parent	= &dev->dev;

	if (KMI_SIZE > 0x1000) {
		printk("KMI_SIZE greater than expected (%x)\n", KMI_SIZE);
		return -1;
	}

	kmi->io		= io;
	if (l4io_request_iomem(dev->res.start, 0x1000,
	                       L4IO_MEM_NONCACHED, (l4_addr_t *)&kmi->base)) {
		printf("l4io_request_mem_region(%lx, 0x1000, 0) failed\n",
		       dev->res.start);
		return -1;
	}
	printf("pl050: got memory %lx, virtual base at %p, IRQ %d\n",
	       dev->res.start, kmi->base, dev->irq[0]);

	kmi->irq	= dev->irq[0];

	serio_register_port(io);
	return 0;

 out:
	kfree(io);
	return ret;
}

static int __init amba_kmi_pre_probe(const char *name, struct amba_device *d)
{
	l4io_resource_t res;
	l4io_device_handle_t dh;
	l4io_resource_handle_t reshandle, tmp;

	if (l4io_lookup_device(name, &dh, 0, &reshandle))
		return -ENODEV;

	tmp = reshandle;
	if (l4io_lookup_resource(dh, L4IO_RESOURCE_IRQ,
	                              &tmp, &res))
		return -ENODEV;
	d->irq[0] = res.start;

	tmp = reshandle;
	if (l4io_lookup_resource(dh, L4IO_RESOURCE_MEM,
	                              &tmp, &res))
		return -ENODEV;
	d->res.start = res.start;
	d->res.end   = res.end;

	strncpy(d->dev.bus_id, name, sizeof(d->dev.bus_id));
	d->dev.bus_id[sizeof(d->dev.bus_id) - 1] = 0;

	return amba_kmi_probe(d, NULL);
}

static int __init amba_kmi_init_k(void)
{
	return amba_kmi_pre_probe("AMBA KMI Kbd", &dev_kmi_k);
}

static int __init amba_kmi_init_m(void)
{
	return amba_kmi_pre_probe("AMBA KMI mou", &dev_kmi_m);
}

static void __exit amba_kmi_exit_k(void)
{
	// XXX: return resources
}
static void __exit amba_kmi_exit_m(void)
{
	// XXX: return resources
}

module_init(amba_kmi_init_k);
module_init(amba_kmi_init_m);
module_exit(amba_kmi_exit_k);
module_exit(amba_kmi_exit_m);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("AMBA KMI controller driver");
MODULE_LICENSE("GPL");
