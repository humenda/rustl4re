/****************************************************************
 * (c) 2005 - 2009 Technische Universitaet Dresden              *
 * This file is part of DROPS, which is distributed under the   *
 * terms of the GNU General Public License 2. Please see the    *
 * COPYING file for details.                                    *
 ****************************************************************/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_tun.h>

#include <l4/lxfuxlibc/lxfuxlc.h>

#include <l4/re/env.h>
#include <l4/util/kip.h>
#include <l4/io/io.h>

struct ux_private {
	int fd;
	int prov_pid;
	int ux_init_done;
};

extern void enable_ux_self(void);

static irqreturn_t ux_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct ux_private *priv = netdev_priv(dev);
	struct sk_buff *skb = dev_alloc_skb(ETH_FRAME_LEN);
	int ret;

	if (!priv->ux_init_done) {
		enable_ux_self();
		priv->ux_init_done = 1;
	}

	skb->dev = dev;

	if ((ret = lx_read(priv->fd, skb->data, ETH_FRAME_LEN)) < 0) {
		if (ret != -EAGAIN)
			printk("uxdev: Error reading packet data: %d\n", ret);
		/* else: ping interrupt, ignore */
		dev_kfree_skb(skb);
		return IRQ_HANDLED;
	}

	skb_put(skb, ret);

	/* ACK interrupt */
	lx_kill(priv->prov_pid, SIGUSR1);

	skb->protocol = eth_type_trans(skb, dev);

	netif_rx(skb);

	return IRQ_HANDLED;
}

static int ux_open(struct net_device *dev)
{
	struct ux_private *priv = netdev_priv(dev);
        int err;

	netif_carrier_off(dev);

	if ((err = request_irq(dev->irq, ux_interrupt,
	                       0, dev->name, dev))) {
		printk("%s: request_irq(%d, ...) failed.\n",
		       dev->name, dev->irq);
		return -ENODEV;
	}

	netif_carrier_on(dev);
	netif_wake_queue(dev);

	printk("%s: interface up.\n", dev->name);

	return 0;
}

static int ux_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ux_private *priv = netdev_priv(dev);

	if (lx_write(priv->fd, skb->data, skb->len) != skb->len) {
		printk("%s: Error writing packet data\n", dev->name);
		return 1;
	}

	dev_kfree_skb(skb);

	dev->trans_start = jiffies;

	return 0;
}

static int ux_close(struct net_device *dev)
{
	free_irq(dev->irq, dev);
	netif_stop_queue(dev);
	netif_carrier_off(dev);

	return 0;
}

static const struct net_device_ops ux_netdev_ops = {
	.ndo_open       = ux_open,
	.ndo_start_xmit = ux_start_xmit,
	.ndo_stop       = ux_close,
};

static int __init ux_init(void)
{
	struct net_device *netdev;
	struct ux_private *priv;
	struct ifreq ifr;
	l4io_device_handle_t devhandle = l4io_get_root_device();
	l4io_device_t iodev;
	l4io_resource_handle_t reshandle;
	int irq = -1;
	int fd = -1;
	int prov_pid = -1;
	int err;

	if (!l4util_kip_kernel_is_ux(l4re_kip()))
		return 1;

	printk("UX Ankh driver firing up...\n");

	while (!l4io_iterate_devices(&devhandle, &iodev, &reshandle)) {
		l4io_resource_t res;
		if (strcmp(iodev.name, "L4UXnet"))
			continue;

		while (!l4io_lookup_resource(devhandle, L4IO_RESOURCE_ANY,
		                             &reshandle, &res)) {
			if (res.type == L4IO_RESOURCE_IRQ)
				irq = res.start;
			if (res.type == L4IO_RESOURCE_PORT) {
				fd       = res.start;
				prov_pid = res.end;
			}
		}
	}

	if (irq == -1 || fd == -1 || prov_pid == -1) {
		printk(KERN_ERR "uxeth: Could not find L4UXnet device\n");
		return 1;
	}

	enable_ux_self();

	netdev = alloc_etherdev(sizeof(struct ux_private));
	if (netdev == NULL) {
		printk(KERN_ERR "uxeth: could not allocate device!\n");
		return 1;
	}

	netdev->netdev_ops = &ux_netdev_ops;

	netdev->dev_addr[0] = 0x04;
	netdev->dev_addr[1] = 0xEA;
	netdev->dev_addr[2] = 0xDD;
	netdev->dev_addr[3] = 0xFF;
	netdev->dev_addr[4] = 0xFF;
	netdev->dev_addr[5] = 0xFE;

	priv = netdev_priv(netdev);

	netdev->irq    = irq;
	priv->fd       = fd;
	priv->prov_pid = prov_pid;

	if ((err = register_netdev(netdev))) {
		printk(KERN_ERR "uxeth: Could not register network device.\n");
		free_netdev(netdev);
		return err;
	}

	printk(KERN_INFO "uxeth: Ready (IRQ: %d, fd: %d, prov_pid: %d)\n",
	       irq, fd, prov_pid);

	return 0;
}

static void __exit ux_exit(void)
{
	printk("uxeth: exit function called, cleanup missing!\n");
}

module_init(ux_init);
module_exit(ux_exit);
