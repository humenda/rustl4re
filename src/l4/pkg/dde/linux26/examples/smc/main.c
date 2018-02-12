#include <l4/dde/linux26/dde26.h> /* l4dde26_*() */
#include <l4/dde/linux26/dde26_net.h> /* l4dde26 networking */
#include <l4/sys/types.h> /* l4_threadid_t */
#include <l4/sys/ipc.h> /* l4_ipc_*() */
#include <l4/sys/kdebug.h> /* enter_kdebug() */
#include <l4/re/env.h>
#include <l4/io/io.h>

#include <linux/netdevice.h> /* struct sk_buff */
#include <linux/pci.h> /* pci_unregister_driver() */
#include <linux/init.h>  // initcall()
#include <linux/delay.h> // msleep()
#include <linux/mii.h> // struct mii_info */
#include <linux/platform_device.h>

#include "arping.h"

extern int arping_verbose;
#define VERBOSE_LOG(fmt, ...) \
	do { \
		if (arping_verbose) printk(fmt, ##__VA_ARGS__); \
	} while (0);

struct pernet_operations __net_initdata loopback_net_ops = {
};

int create_devices(void)
{
  int ret, i;
  l4io_device_handle_t l4io_handle = 0;
  l4io_device_t l4io_dev;
  l4io_resource_t res_desc;
  l4io_resource_handle_t res_handle = 0;
  int num_devs = 0;

  while (1)
    {
      ret = l4io_iterate_devices(&l4io_handle, &l4io_dev, &res_handle);
      if (ret)
	{
	  return num_devs;
	}

      //if (l4io_dev.type == L4IO_DEVICE_NET)
	{
	  printk("Create Linux network device structure for %s network device\n", l4io_dev.name);
	  struct platform_device *platform_dev = kmalloc(sizeof(struct platform_device), GFP_KERNEL);
	  char *name = kmalloc(L4VBUS_DEV_NAME_LEN, GFP_KERNEL);
	  strncpy((char *)name, l4io_dev.name, L4VBUS_DEV_NAME_LEN);
	  platform_dev->name = name;
	  platform_dev->id = -1;
	  platform_dev->dev.platform_data = 0;
	  platform_dev->num_resources = l4io_dev.num_resources;

	  struct resource *platform_res = kmalloc(sizeof(struct resource) * platform_dev->num_resources, GFP_KERNEL);
	  for (i = 0; i < platform_dev->num_resources; i++)
	    {
	      ret = l4io_lookup_resource(l4io_handle, L4IO_RESOURCE_ANY, &res_handle, &res_desc);
	      if (ret)
		{
		  printk("Adding resource failed\n");
		  return num_devs;
		}

	      printk("Add resource %d %lx %lx\n", res_desc.type, res_desc.start, res_desc.end);
	      platform_res[i].flags = (res_desc.type == L4IO_RESOURCE_MEM) ? IORESOURCE_MEM : IORESOURCE_IRQ;
	      platform_res[i].start = res_desc.start;
	      platform_res[i].end = res_desc.end;
	    }
	  platform_dev->resource = platform_res;

	  platform_add_devices(&platform_dev, 1);
	  num_devs++;
	}
    }

  return num_devs;
}

extern int arping(void);
struct net_device *net_dev = 0;

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
			if (err == 0)
			  {
			    printk("set netork device\n");
			  net_dev = dev;
			  }
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

int handle_icmp_packet(struct sk_buff *skb);

static int net_rx_handle(struct sk_buff *skb)
{
  printk("shuuup\n");
    skb_push(skb, skb->dev->hard_header_len);

    handle_icmp_packet(skb);
    
    kfree_skb(skb);

    VERBOSE_LOG("freed skb, returning from netif_rx\n");
    return NET_RX_SUCCESS;
}

struct sk_buff *arp_create(int type, int ptype, __be32 dest_ip,
			   struct net_device *dev, __be32 src_ip,
			   const unsigned char *dest_hw,
			   const unsigned char *src_hw,
			   const unsigned char *target_hw);

#define ARPOP_REQUEST 1

int main(int argc, char **argv);
int main(int argc, char **argv)
{
  	int ret;
	l4_addr_t virt_base = 0;
	struct sk_buff *sk = 0;

	l4dde26_softirq_init();

	printk("Initializing skb subsystem\n");
	skb_init();

	int num_devs = create_devices();
	printk("Created %d device(s)\n", num_devs);

	// linux driver subsystem should match the correct driver
	
	printk("Setting rx callback @ %p\n", net_rx_handle);
	l4dde26_register_rx_callback(net_rx_handle);

	printk("Opening nw devs.\n");
	open_nw_dev();
	printk("dev is up and ready.\n");

	arping();
#if 0
	int dest_ip = 0xffff;
	int src_ip = 0x1234;
	while (1)
	  {
	    sk = arp_create(ARPOP_REQUEST, ETH_P_ARP, dest_ip, net_dev, src_ip, 0, 0, 0);
	    net_dev->hard_start_xmit(sk, net_dev);
	    printk("created ARP request\n");
	    l4_sleep(1);
	  }
#endif

	close_nw_dev();

	printk("shut down driver\n");

	return 0;
}
