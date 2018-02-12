#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include "linux_glue.h"

#include <l4/dde/linux26/dde26_net.h>
#undef __always_inline
#include <assert.h>

#define DEBUG(DBG, msg, ...) \
	do { if (DBG) \
			printk(msg, ##__VA_ARGS__); \
	} while (0);

#define ANKH_DEBUG_INIT     1
#define ANKH_DEBUG_PACKET   0

int ankh_debug_linuxglue = 0;
void ankh_set_debug()   { ++ankh_debug_linuxglue; }
void ankh_unset_debug() { --ankh_debug_linuxglue; }

/*****************************************************************************
 * Open the "real" network devices. Return the number of opened interfaces.
 *****************************************************************************/
int open_network_devices(int promisc)
{
	struct net_device *dev;
	struct net *net;
	int err = 0, cnt = 0;

	DEBUG(ANKH_DEBUG_INIT, "%s() \n", __func__);

	for_each_net(net) {
		for_each_netdev(net, dev)
		{
			DEBUG(ANKH_DEBUG_INIT, "opening %s\n", dev->name);

			// beam us to promiscuous mode, so that we can receive packets that
			// are not meant for the NIC's MAC address --> we need that, because
			// ORe clients have different MAC addresses
			if (promisc && netdev_set_promisc(dev) == 0)
					DEBUG(ANKH_DEBUG_INIT, "set interface to promiscuous mode.\n");

			err = dev_open(dev);
			if (err)
			{
				DEBUG(ANKH_DEBUG_INIT, "error opening %s : %d\n", dev->name, err);
				return err;
			}
			else // success
				netdev_add(dev);

		cnt++;
		//xmit_lock_add(dev->name);
		}
	}

	return cnt;
}


static int net_recv_irq(struct sk_buff *skb)
{
	int i;
	
	/*
	 * Special case: The loopback device is handled
	 * by local packet delivery beforehand. The loopback
	 * driver however, tries to directly call the recv() routine
	 * upon xmit(), too.
	 */
	if (!strcmp(skb->dev->name, "lo"))
		return NET_RX_SUCCESS;

	/*
	 * Push eth header back, so that skb->data is the raw packet.
	 */
	skb_push(skb, ETH_HLEN);

	/*
	 * Hand to upper layers.
	 */
#if 0
	DECLARE_MAC_BUF(mac);
	printk("%s\n", print_mac(mac, skb->data));
#endif
	packet_deliver(skb->data, skb->len, skb->dev->name, 0);

	/*
	 * Need to release skb here.
	 */
	kfree_skb(skb);

	return NET_RX_SUCCESS;
}


static void _init_linux_glue(void)
{
	l4dde26_softirq_init();
	skb_init();

	l4dde26_register_rx_callback(net_recv_irq);
}
subsys_initcall(_init_linux_glue);

/*****************************************************************************
 * wrapper functions for accessing net_device members
 *****************************************************************************/

#define ND(p) ((struct net_device *)p)

int netdev_irq(void *netdev)
{ return ND(netdev)->irq; }


char const *netdev_name(void *netdev)
{ return dev_name(&ND(netdev)->dev); }


int netdev_set_promisc(void *netdev)
{
	int err;
	struct net_device *dev = ND(netdev);
	if ((err = dev_change_flags(dev, dev->flags | IFF_PROMISC)) != 0)
		DEBUG(ANKH_DEBUG_INIT, "%s could not be set to promiscuous mode.\n",
		                      dev->name);

	return err;
}


int netdev_unset_promisc(void *netdev)
{
	int err;
	struct net_device *dev = ND(netdev);
	if ((err = dev_change_flags(dev, dev->flags & ~IFF_PROMISC)) != 0)
		DEBUG(ANKH_DEBUG_INIT, "%s could not be set to promiscuous mode.\n",
		                      dev->name);

	return err;
}


void *alloc_dmaable_buffer(unsigned size)
{
	return kmalloc(size, GFP_KERNEL);
}


int netdev_get_promisc(void *netdev)
{ return ND(netdev)->flags & IFF_PROMISC; }


char *netdev_dev_addr(void *netdev)
{ return ND(netdev)->dev_addr; }


int netdev_mtu(void *netdev)
{ return ND(netdev)->mtu; }


int netdev_xmit(void *netdev, char *addr, unsigned len)
{
	// XXX could we pass 0 as length here? data netdev is set
	//     below anyway
	struct sk_buff *skb = alloc_skb(len, GFP_KERNEL);
	assert(skb);

	skb->data = addr; // XXX
	skb_put(skb, len);
	skb->dev  = ND(netdev);

	while (netif_queue_stopped(ND(netdev)))
		msleep(1);

	int err;
	if (ND(netdev)->netdev_ops)
		err = ND(netdev)->netdev_ops->ndo_start_xmit(skb, ND(netdev));
	else
		err = ND(netdev)->hard_start_xmit(skb, ND(netdev));

	return err;
}
