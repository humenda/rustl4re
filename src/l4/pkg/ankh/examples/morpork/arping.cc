#include <l4/sys/types.h>
#include <cstdio>
#include <cstring>
#include <cassert>

struct ethernet_hdr
{
    unsigned char dest[6];
    unsigned char src[6];
    unsigned char type[2];
};


static void __attribute__((unused)) dump_packet(char *p, unsigned sz)
{
	for (unsigned i=0; i < sz; ++i) {
		printf("%02X ", (unsigned char)*(p+i));
		if (i % 20 == 19)
			printf("\n");
	}
	printf("\n");
}


static char const * ethertype_str(unsigned int type)
{
	switch (type)
	{
		case 0x0800:
			return "IP";
		case 0x0806:
			return "ARP";
		case 0x814C:
			return "SNMP";
		case 0x86DD:
			return "IPv6";
		case 0x880B:
			return "PPP";
	}

	return "unknown";
}


static char const *ipprot_str(unsigned char protocol)
{
	switch(protocol)
	{
		case 0x01:
			return "ICMP";
		case 0x06:
			return "TCP";
		case 0x11:
			return "UDP";
	}
	return "unknown";
}


#define VERBOSE_LOG(fmt, ...) \
    do { \
        if (arping_verbose) printf(fmt, ##__VA_ARGS__); \
    } while (0);

/*
 * stolen from Linux
 */
#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]
#define NIPQUAD_FMT "%u.%u.%u.%u"


bool arping_verbose = false;

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

#define ntohs(x) \
  (l4_uint16_t)(  (((l4_uint16_t)(x) & 0x00FFU) << 8) \
                | (((l4_uint16_t)(x) & 0xFF00U) >> 8) \
               )


extern "C" int handle_icmp_packet(char *packet, unsigned size __attribute__((unused)),
								  char *buffer);
extern "C" int handle_icmp_packet(char *packet, unsigned size __attribute__((unused)),
								  char *buffer)
{
    static const int ICMP_REPLY = 0;
    static const int PROT_ICMP = 1;
    static const int ICMP_REQ = 8;
    static const int ETH_ALEN = 6;

    struct ethernet_hdr *eth = NULL;
    struct ethernet_hdr *e   = NULL;
    struct ip_hdr *ip        = NULL;
    struct ip_hdr *iphdr     = NULL;
    struct icmp_hdr *icmp    = NULL;
    struct icmp_hdr *icmp2   = NULL;
    int ver, len;

    eth = (struct ethernet_hdr *)packet;
    VERBOSE_LOG("dest mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
                eth->dest[0], eth->dest[1], eth->dest[2],
                eth->dest[3], eth->dest[4], eth->dest[5]);
    VERBOSE_LOG("src mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
                eth->src[0], eth->src[1], eth->src[2],
                eth->src[3], eth->src[4], eth->src[5]);
    VERBOSE_LOG("type field = %02x%02x\n", eth->type[0], eth->type[1]);
    if (eth->type[0] != 0x08 || eth->type[1] != 0x00) {
        VERBOSE_LOG("unknown ethernet packet type %02x:%02x (%s)!\n",
		            (unsigned)eth->type[0], (unsigned)eth->type[1],
		            ethertype_str((eth->type[0] << 8 | eth->type[1]) & 0xFFFF));
        return -1;
    }

    ip = (struct ip_hdr *)(packet + sizeof(struct ethernet_hdr));
    VERBOSE_LOG("protocol = %02x (2x?)\n", ip->protocol);
    if (ip->protocol != PROT_ICMP)
    {
        VERBOSE_LOG("Unknown protocol type: %x (%s)\n", (unsigned)ip->protocol, ipprot_str(ip->protocol));
        return -1;
    }

    VERBOSE_LOG("ICMP packet!\n");
    ver = ip->version_length >> 4;
    len = ip->version_length & 0x0F;
    VERBOSE_LOG("IP version = %d, length = %d\n", ver, len);

    VERBOSE_LOG("src IP: "NIPQUAD_FMT"\n", NIPQUAD(ip->src_ip));
    VERBOSE_LOG("dest IP: "NIPQUAD_FMT"\n", NIPQUAD(ip->dest_ip));

    icmp = (struct icmp_hdr *)(packet + sizeof(struct ethernet_hdr)
            + sizeof(struct ip_hdr));

    if (icmp->type != ICMP_REQ)
    {
        printf("This is no ICMP request: %x\n", (unsigned)icmp->type);
        return -1;
    }
    VERBOSE_LOG("Hey this is an ICMP request just for me. :)\n");
    VERBOSE_LOG("ICMP type : %d\n", icmp->type);
    VERBOSE_LOG("ICMP code : %d\n", icmp->code);
    VERBOSE_LOG("ICMP seq  : %d\n", ntohs(icmp->seq_num));

    e = (struct ethernet_hdr *)buffer;
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

    iphdr  = (struct ip_hdr *)(buffer + sizeof(struct ethernet_hdr));
    *iphdr = *ip;
    // also switch src and dest
    iphdr->src_ip  = ip->dest_ip;
    iphdr->dest_ip = ip->src_ip;
    VERBOSE_LOG("src IP: "NIPQUAD_FMT"\n", NIPQUAD(iphdr->src_ip));
    VERBOSE_LOG("dest IP: "NIPQUAD_FMT"\n", NIPQUAD(iphdr->dest_ip));

    icmp2 = (struct icmp_hdr *)(buffer + sizeof(struct ethernet_hdr)
                                + sizeof(struct ip_hdr));
    *icmp2     = *icmp;
    icmp2->type = ICMP_REPLY;

    return 0;
}
