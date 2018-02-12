#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/l4int.h>

/*
 * Types
 */
typedef union
{
	unsigned char a[6];
} mac_addr;


typedef union
{
	l4_uint32_t a;
} ip_addr;


/*
 * ETHERNET
 */
typedef struct _eth_hdr {
    mac_addr dst;
    mac_addr src;
    l4_uint16_t type;
} eth_hdr;


/*
 * ARP
 */

enum arp_ops {
	arp_op_request = 1,
	arp_op_reply   = 2,
	arp_op_rarp_request = 3,
	arp_op_rarp_reply = 4,
};

typedef struct  __attribute__((packed))
{
	l4_uint16_t htype;
	l4_uint16_t ptype;
	l4_uint8_t  hlen;
	l4_uint8_t  plen;
	l4_uint16_t op;
	mac_addr    snd_hw_addr;
	ip_addr     snd_prot_addr;
	mac_addr    target_hw_addr;
	ip_addr     target_prot_addr;
} arp_hdr;


/*
 * IP
 */

enum ip_proto {
	ip_proto_icmp = 1,
	ip_proto_tcp  = 6,
	ip_proto_udp  = 17,
};

typedef struct  __attribute__((packed)){
	l4_uint8_t  ver_hlen;          // version 4, hlen 4
	l4_uint8_t  services;
	l4_uint16_t plen;
	l4_uint16_t id;
	l4_uint16_t flags_frag_offset; // flags 3, offset 13
	l4_uint8_t  ttl;
	l4_uint8_t  proto;
	l4_uint16_t checksum;
	ip_addr     src_addr;
	ip_addr     dest_addr;
} ip_hdr;


/*
 * UDP
 */

enum udp_ports {
	udp_port_reserved = 0,
	udp_port_echo = 7,
	udp_port_dns_srv = 53,
	udp_port_bootp_srv = 67,
	udp_port_bootp_clnt = 68,
};

typedef struct __attribute__((packed))
{
	l4_uint16_t src_port;
	l4_uint16_t dst_port;
	l4_uint16_t length;
	l4_uint16_t checksum;
} udp_hdr;

/*
 * ICMP
 */
typedef struct
{
} icmp_hdr;


/*
 * TCP
 */
typedef struct
{
} tcp_hdr;


/*
 * DHCP
 */

typedef struct
{
	l4_uint8_t  op;
	l4_uint8_t  hytpe;
	l4_uint8_t  hlen;
	l4_uint8_t  hops;
	l4_uint32_t xid;
	l4_uint16_t secs;
	l4_uint16_t flags;
	ip_addr     clnt_addr;
	ip_addr     your_addr;
	ip_addr     server_addr;
	ip_addr     gateway_addr;
	char        chaddr[16];
	char        sname[64];
	char        bootfile[128];
} dhcp;

typedef struct
{
	l4_uint16_t  ident; // request identifier
	l4_uint8_t   qr:1;  // 0 = query, 1 = response
	l4_uint8_t   opcode:4; // 0 - 5
	l4_uint8_t   aa:1; // authoritative answer y/n
	l4_uint8_t   tc:1; // truncated y/n
	l4_uint8_t   rd:1; // recursion desired
	l4_uint8_t   ra:1; // recursion available
	l4_uint8_t    z:1; // --
	l4_uint8_t   ad:1; // authenticated data
	l4_uint8_t   cd:1; // checking disabled
	l4_uint8_t   rcode:4; // return code
	l4_uint16_t  tot_questions; // # of questions
	l4_uint16_t  tot_answer_rr; // # of answers
	l4_uint16_t  tot_auth_rr; // # of authority resource records
	l4_uint16_t  tot_add_rr; // # of addtional resource records
} dns_t;

enum dns_ops
{
	dns_query,
	dns_iquery,
	dns_status,
	dns_reserved,
	dns_notify,
	dns_update,
};

/*
 * Config stuff
 */
#define PA_ARP 0

#define PA_IP  (PA_ICMP || PA_TCP || PA_UDP)

#define PA_ICMP  0
#define PA_TCP   0

#define PA_UDP   (PA_DHCP || PA_BOOTP || PA_DNS)
#define PA_DHCP  0
#define PA_BOOTP 0
#define PA_DNS   0


/*
 * The analysis function.
 */
__BEGIN_DECLS

void packet_analyze(char *p, unsigned len);

__END_DECLS
