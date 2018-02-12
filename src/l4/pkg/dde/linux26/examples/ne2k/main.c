#include <l4/dde/linux26/dde26.h> /* l4dde26_*() */
#include <l4/dde/linux26/dde26_net.h> /* l4dde26 networking */
#include <l4/sys/types.h> /* l4_threadid_t */
#include <l4/sys/ipc.h> /* l4_ipc_*() */
#include <l4/sys/kdebug.h> /* enter_kdebug() */
#include <l4/re/env.h>

#include <linux/netdevice.h> /* struct sk_buff */
#include <linux/pci.h> /* pci_unregister_driver() */
#include <linux/init.h>  // initcall()
#include <linux/delay.h> // msleep()

#include "arping.h"

#include "8390.h" /* that's what we are */

extern int arping_verbose;
#define VERBOSE_LOG(fmt, ...) \
	do { \
		if (arping_verbose) printk(fmt, ##__VA_ARGS__); \
	} while (0);

extern struct pci_driver ne2k_driver;
extern int arping(void);

void open_nw_dev(void);
void open_nw_dev()
{
	struct net_device *dev;
	struct net *net;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			int err = 0;
			printk("dev: '%s'\n", dev->name);
			printk("MAC: "mac_fmt"\n", mac_str(dev->dev_addr));

			err = dev_open(dev);
		}
	}
	read_unlock(&dev_base_lock);
}

void close_nw_dev(void);
void close_nw_dev(void)
{
	struct net_device *dev;
	struct net *net;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			int err = 0;

			err = dev_close(dev);
			printk("closed %s\n", dev->name);
		}
	}
	read_unlock(&dev_base_lock);
}

static int net_rx_handle(struct sk_buff *skb)
{
    skb_push(skb, skb->dev->hard_header_len);

	struct arping_elem *e = kmalloc(sizeof(struct arping_elem), GFP_KERNEL);
	e->skb = skb;
	skb_get(skb);
	e->next = NULL;

	if (arping_list == NULL)
		arping_list = e;
	else {
		struct arping_elem *f = arping_list;
		while (f->next)
			f = f->next;
		f->next = e;
	}

	ddekit_sem_up(arping_semaphore);
    
    kfree_skb(skb);

	VERBOSE_LOG("freed skb, returning from netif_rx\n");
    return NET_RX_SUCCESS;
}

//subsys_initcall(l4dde26_init_pci);

int main(int argc, char **argv);
int main(int argc, char **argv)
{
	l4dde26_softirq_init();

	printk("Initializing skb subsystem\n");
	skb_init();

	printk("Setting rx callback @ %p\n", net_rx_handle);
	l4dde26_register_rx_callback(net_rx_handle);

	printk("Opening nw devs.\n");
	open_nw_dev();
	printk("dev is up and ready.\n");

	arping();

	close_nw_dev();

	pci_unregister_driver(&ne2k_driver);
	printk("shut down driver\n");

	return 0;
}
