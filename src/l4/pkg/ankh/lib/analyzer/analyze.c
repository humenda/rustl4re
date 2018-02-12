#include <l4/ankh/packet_analyzer.h>
#include <l4/util/assert.h>
#include <stdio.h>

#include <arpa/inet.h>
#include "etypes.h"

#define mac_fmt       "%02X:%02X:%02X:%02X:%02X:%02X"
#define mac_str(mac)  (unsigned char)((mac)[0]), (unsigned char)((mac)[1]), \
                      (unsigned char)((mac)[2]), (unsigned char)((mac)[3]), \
                      (unsigned char)((mac)[4]), (unsigned char)((mac)[5])
#define macbuf_size 18


#define ip_fmt        "%d.%d.%d.%d"
#define ip_str(ip)    ((unsigned char*)(ip))[0], \
                      ((unsigned char*)(ip))[1], \
                      ((unsigned char*)(ip))[2], \
                      ((unsigned char*)(ip))[3]
#define ipbuf_size  16


static void print_mac(char *buf, mac_addr *e)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_NOT_NULL(e);

	snprintf(buf, macbuf_size, mac_fmt, mac_str(e->a));
}


static void print_ip(char *buf, ip_addr *i)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_NOT_NULL(i);
	l4_uint32_t addr = i->a;

	snprintf(buf, ipbuf_size, ip_fmt, ip_str(&addr));
}


static l4_uint16_t parse_eth(eth_hdr *e)
{
	char addr1[macbuf_size];
	char addr2[macbuf_size];

	l4_uint16_t type = ntohs(e->type);
	switch(type) {
		case ETHERTYPE_ARP:
			if (!PA_ARP)
				return 0;
			break;
		case ETHERTYPE_IP:
			if (!PA_IP)
				return 0;
			break;
		default:
			return 0;
	}

	print_mac(addr1, &e->dst);
	print_mac(addr2, &e->src);

	printf("[ETH] dst %s src %s type ", addr1, addr2);
	switch(ntohs(e->type)) {
		case ETHERTYPE_IP:
			printf("IP\n");
			break;
		case ETHERTYPE_ARP:
			printf("ARP\n");
			break;
		default:
			printf("%x??\n", ntohs(e->type));
			break;
	}

	return type;
}


static void parse_arp(arp_hdr *a)
{
	char mac1[macbuf_size];
	char mac2[macbuf_size];
	char ip1[ipbuf_size];
	char ip2[ipbuf_size];

	print_mac(mac1, &a->snd_hw_addr);
	print_mac(mac2, &a->target_hw_addr);
	print_ip(ip1, &a->snd_prot_addr);
	print_ip(ip2, &a->target_prot_addr);

	printf("  [ARP]");
	switch(ntohs(a->op))
	{
		case arp_op_request:
			printf(" REQ from %s (%s) for %s\n", mac1, ip1, ip2);
			break;
		case arp_op_reply:
			printf(" REPLY from %s (%s), target = %s (%s)\n",
			       mac1, ip1, mac2, ip2);
			break;
		case arp_op_rarp_request:
			printf(" RARP REQ from %s (%s) for %s\n", mac1, ip1, ip2);
			break;
		case arp_op_rarp_reply:
			printf(" RARP REPLY from %s (%s), target = %s (%s)\n",
			       mac1, ip1, mac2, ip2);
			break;
	}
}


static void parse_dhcp_srv(dhcp *d)
{
	char ip1[ipbuf_size];
	char ip2[ipbuf_size];
	char ip3[ipbuf_size];
	char ip4[ipbuf_size];

	print_ip(ip1, &d->clnt_addr);
	print_ip(ip2, &d->your_addr);
	print_ip(ip3, &d->server_addr);
	print_ip(ip4, &d->gateway_addr);

	printf("    [DHCP/Srv] op %x ", d->op);
	printf("CLNT %s YOUR %s\n", ip1, ip2);
	printf("                SRV %s GW %s\n", ip3, ip4);
}


static void parse_dhcp_clnt(dhcp *d)
{
	char ip1[ipbuf_size];
	char ip2[ipbuf_size];
	char ip3[ipbuf_size];
	char ip4[ipbuf_size];

	print_ip(ip1, &d->clnt_addr);
	print_ip(ip2, &d->your_addr);
	print_ip(ip3, &d->server_addr);
	print_ip(ip4, &d->gateway_addr);

	printf("    [DHCP/Clnt] op %x ", d->op);
	printf("CLNT %s YOUR %s\n", ip1, ip2);
	printf("                SRV %s GW %s\n", ip3, ip4);
}


static char const *udp_port_str(l4_uint16_t p)
{
	switch(p)
	{
		case udp_port_reserved:
			return "reserved";
		case udp_port_echo:
			return "echo";
		case udp_port_bootp_srv:
			return "BOOTP/Srv";
		case udp_port_bootp_clnt:
			return "BOOTP/Clnt";
		case udp_port_dns_srv:
			return "DNS/Srv";
		default:
			break;
	}

	return "??";
}


static void parse_dns_srv(dns_t *d)
{
	printf("    [DNS/Srv] id %x qr %x (%s) op %x (",
		   d->ident, d->qr, d->qr ? "response" : "query",
		   d->opcode);
	switch (d->opcode) {
		case dns_query:
			printf("query");
			break;
		case dns_iquery:
			printf("inv. query");
			break;
		case dns_status:
			printf("status");
			break;
		case dns_notify:
			printf("notify");
			break;
		case dns_update:
			printf("update");
			break;
		default:
			printf("reserved");
			break;
	}
	printf(")\n");
}


static void parse_udp(udp_hdr *u)
{
	l4_uint16_t psrc = ntohs(u->src_port);
	l4_uint16_t pdst =  ntohs(u->dst_port);
	printf("    [UDP] src %d(%s), dest %d(%s), len %x, cs %x\n",
		   psrc, udp_port_str(psrc), pdst, udp_port_str(pdst),
	       ntohs(u->length), ntohs(u->checksum));

	switch(pdst) {
		case udp_port_bootp_srv:
			if (PA_DHCP || PA_BOOTP)
				parse_dhcp_srv((dhcp*)((char*)u + sizeof(udp_hdr)));
			break;
		case udp_port_bootp_clnt:
			if (PA_DHCP || PA_BOOTP)
				parse_dhcp_clnt((dhcp*)((char*)u + sizeof(udp_hdr)));
			break;
		case udp_port_dns_srv:
			if (PA_DNS)
				parse_dns_srv((dns_t *)((char*)u + sizeof(udp_hdr)));
	}
}


static void parse_icmp(icmp_hdr *i)
{
	(void)i;
	// XXX
}


static void parse_tcp(tcp_hdr *t)
{
	(void)t;
	// XXX
}


static char const * ip_proto_str(l4_uint8_t prot)
{
	switch(prot)
	{
		case ip_proto_icmp:
			return "ICMP";
		case ip_proto_tcp:
			return "TCP";
		case ip_proto_udp:
			return "UDP";
	}

	return "??";
}

static void parse_ip(ip_hdr *ip)
{
	char ip1[ipbuf_size];
	char ip2[ipbuf_size];

	print_ip(ip1, &ip->src_addr);
	print_ip(ip2, &ip->dest_addr);

	unsigned char hlen = (ip->ver_hlen & 0x0F) * sizeof(int);

	printf("  [IP] version %d, hlen %d, services %d, plen %d, id %d\n",
	       (ip->ver_hlen & 0xF0) >> 4,
	       (ip->ver_hlen & 0x0F),
	       ip->services,
	       ntohs(ip->plen),
	       ntohs(ip->id));
	printf("       flags %x, offset %x, ttl %d, proto %s (%d)\n",
	       (ntohs(ip->flags_frag_offset) & 0x70000000) >> 13,
	       (ntohs(ip->flags_frag_offset) & 0x1FFFFFFF),
	       ip->ttl, ip_proto_str(ip->proto), ip->proto);
	printf("       csum %x, src %s, dest %s\n",
	       ntohs(ip->checksum), ip1, ip2);

	switch(ip->proto)
	{
		case ip_proto_icmp:
			if (PA_ICMP) parse_icmp((icmp_hdr*)((char*)ip + hlen));
			break;
		case ip_proto_tcp:
			if (PA_TCP)  parse_tcp((tcp_hdr*)((char*)ip + hlen));
			break;
		case ip_proto_udp:
			if (PA_UDP) parse_udp((udp_hdr*)((char*)ip + hlen));
			break;
		default:
			break;
	}
}


void packet_analyze(char *p, unsigned len)
{
	ASSERT_NOT_NULL(p);
	ASSERT_GREATER_EQ(len, sizeof(eth_hdr));

	eth_hdr *eth = (eth_hdr*)p;

	l4_uint16_t type = parse_eth(eth);
	if (!type)
		return;

	switch(type)
	{
		case ETHERTYPE_ARP:
			parse_arp((arp_hdr*)(p + sizeof(eth_hdr)));
			break;
		case ETHERTYPE_IP:
			parse_ip((ip_hdr*)(p + sizeof(eth_hdr)));
			break;
	}
}
