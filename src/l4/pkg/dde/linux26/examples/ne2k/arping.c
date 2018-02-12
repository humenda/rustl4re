/****************************************************************
 * (c) 2007 Technische Universitaet Dresden                     *
 * This file is part of DROPS, which is distributed under the   *
 * terms of the GNU General Public License 2. Please see the    *
 * COPYING file for details.                                    *
 ****************************************************************/

#include <l4/util/util.h>
#include <l4/util/l4_macros.h>
#include <l4/sys/ipc.h>

#include <linux/netdevice.h>
#include <linux/if_ether.h>

#include "arping.h"

#define PROT_ICMP         1
#define ICMP_REPLY        0
#define ICMP_REQ          8
#define ETH_ALEN          6

/* configuration */
int arping_verbose = 0;  // verbose

#define VERBOSE_LOG(fmt, ...) \
	do { \
		if (arping_verbose) printk(fmt, ##__VA_ARGS__); \
	} while (0);

l4_ssize_t l4libc_heapsize = 32 * 1024;

static unsigned char broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static int exit_somewhen = 0;


struct ethernet_hdr
{
	unsigned char dest[6];
	unsigned char src[6];
	unsigned char type[2];
};


struct ip_hdr
{
	char          version_length;
	char          type;
	l4_int16_t    length;
	l4_int16_t    id;
	l4_int16_t    flags_offset;
	char          ttl;
	char          protocol;
	l4_int16_t    checksum;
	l4_int32_t    src_ip;
	l4_int32_t    dest_ip;
};


struct icmp_hdr
{
	char type;
	char code;
	l4_uint16_t checksum;
	l4_uint16_t id;
	l4_uint16_t seq_num;
};


int handle_icmp_packet(struct sk_buff *skb);
int handle_icmp_packet(struct sk_buff *skb)
{
	char *data = skb->data;
	struct ethernet_hdr *eth = NULL;
	struct ethernet_hdr *e   = NULL;
	struct ip_hdr *ip        = NULL;
	struct ip_hdr *iphdr     = NULL;
	struct icmp_hdr *icmp    = NULL;
	struct icmp_hdr *icmp2   = NULL;
	int ver, len;
	struct sk_buff *snd_skb  = NULL;

	eth = (struct ethernet_hdr *)data;
	VERBOSE_LOG("dest mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
	            eth->dest[0], eth->dest[1], eth->dest[2],
	            eth->dest[3], eth->dest[4], eth->dest[5]);
	VERBOSE_LOG("src mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
	            eth->src[0], eth->src[1], eth->src[2],
	            eth->src[3], eth->src[4], eth->src[5]);
	VERBOSE_LOG("type field = %02x%02x\n", eth->type[0], eth->type[1]);
	if (eth->type[0] != 0x08 || eth->type[1] != 0x00) {
		printk("unknown ethernet packet type!\n");
		return -1;
	}

	ip = (struct ip_hdr *)(data + sizeof(struct ethernet_hdr));
	VERBOSE_LOG("protocol = %02x (2x?)\n", ip->protocol, PROT_ICMP);
	if (ip->protocol != PROT_ICMP)
	{
		printk("Unknown packet type.\n");
		return -1;
	}

	VERBOSE_LOG("ICMP packet!\n");
	ver = ip->version_length >> 4;
	len = ip->version_length & 0x0F;
	VERBOSE_LOG("IP version = %d, length = %d\n", ver, len);

	VERBOSE_LOG("src IP: "NIPQUAD_FMT"\n", NIPQUAD(ip->src_ip));
	VERBOSE_LOG("dest IP: "NIPQUAD_FMT"\n", NIPQUAD(ip->dest_ip));

	icmp = (struct icmp_hdr *)(data + sizeof(struct ethernet_hdr)
	        + sizeof(struct ip_hdr));

	if (icmp->type != ICMP_REQ)
	{
		printk("This is no ICMP request.\n");
		return -1;
	}
	VERBOSE_LOG("Hey this is an ICMP request just for me. :)\n");
	VERBOSE_LOG("ICMP type : %d\n", icmp->type);
	VERBOSE_LOG("ICMP code : %d\n", icmp->code);
	VERBOSE_LOG("ICMP seq  : %d\n", ntohs(icmp->seq_num));

	snd_skb = alloc_skb(skb->len + skb->dev->hard_header_len, GFP_KERNEL);
	memcpy(snd_skb->data, skb->data, skb->len);
	
	e = (struct ethernet_hdr *)snd_skb->data;
	memcpy(e->src, eth->dest, ETH_ALEN);
	memcpy(e->dest, eth->src, ETH_ALEN);
	VERBOSE_LOG("dest mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
	            e->dest[0], e->dest[1], e->dest[2],
	            e->dest[3], e->dest[4], e->dest[5]);
	VERBOSE_LOG("src mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
	            e->src[0], e->src[1], e->src[2],
	            e->src[3], e->src[4], e->src[5]);
	e->type[0] = 0x08;
	e->type[1] = 0x00;

	iphdr  = (struct ip_hdr *)(snd_skb->data + sizeof(struct ethernet_hdr));
	*iphdr = *ip;
	// also switch src and dest
	iphdr->src_ip  = ip->dest_ip;
	iphdr->dest_ip = ip->src_ip;
	VERBOSE_LOG("src IP: "NIPQUAD_FMT"\n", NIPQUAD(iphdr->src_ip));
	VERBOSE_LOG("dest IP: "NIPQUAD_FMT"\n", NIPQUAD(iphdr->dest_ip));

	icmp2 = (struct icmp_hdr *)(snd_skb->data + sizeof(struct ethernet_hdr)
	                            + sizeof(struct ip_hdr));
	*icmp2     = *icmp;
	icmp2->type = ICMP_REPLY;

	snd_skb->dev = skb->dev;
	snd_skb->len = skb->len;

	VERBOSE_LOG("sending reply\n");
	skb->dev->hard_start_xmit(snd_skb, skb->dev);
	VERBOSE_LOG("done\n");

	return 0;
}

ddekit_sem_t *arping_semaphore  = NULL;
struct arping_elem *arping_list = NULL;

int arping(void)
{
	arping_semaphore = ddekit_sem_init(0);

	while(1)
	{
		ddekit_sem_down(arping_semaphore);
		struct arping_elem *elem = arping_list;
		arping_list = arping_list->next;

		/* parse packet */
		int err = handle_icmp_packet(elem->skb);
		VERBOSE_LOG("handle_icmp_packet: %d\n", err);

		kfree_skb(elem->skb);
		kfree(elem);
	}
}

