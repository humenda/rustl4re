#ifndef GRUB_H
#define GRUB_H

#include "osdep.h"
#include "byteswap.h"
#include "in.h"
#include "ip.h"
#include "udp.h"
#include "if_ether.h"
#include "latch.h"
#include "io.h"
#include "nic.h"
#ifndef ANKH
#include <shared.h>
#include <stdlib.h>
#include <stdio.h>
#endif

#define K_ESC		'\033'
#define K_EOF		'\04'  /* Ctrl-D */
#define K_INTR		'\03'  /* Ctrl-C */

#ifndef	MAX_RPC_RETRIES
#define MAX_RPC_RETRIES		20
#endif

/* Link configuration time in tenths of a second */
#ifndef VALID_LINK_TIMEOUT
#define VALID_LINK_TIMEOUT	100 /* 10.0 seconds */
#endif

/* Inter-packet retry in ticks */
#ifndef TIMEOUT
#define TIMEOUT			(10*TICKS_PER_SEC)
#endif

#ifndef	NULL
#define NULL	((void *)0)
#endif


#define ARP_CLIENT	0
#define ARP_SERVER	1
#define ARP_GATEWAY	2
#define MAX_ARP		ARP_GATEWAY+1

#define IGMP_SERVER	0
#define MAX_IGMP	IGMP_SERVER+1

#define	RARP_REQUEST	3
#define	RARP_REPLY	4


#define MULTICAST_MASK    0xF0000000
#define MULTICAST_NETWORK 0xE0000000

struct arptable_t {
	struct in_addr ipaddr;
	uint8_t node[6];
};

struct igmptable_t {
	struct in_addr group;
	unsigned long time;
};

#define	KERNEL_BUF	(BOOTP_DATA_ADDR->bootp_reply.bp_file)

#define	FLOPPY_BOOT_LOCATION	0x7c00
/* Must match offsets in loader.S */
#define ROM_SEGMENT		0x1fa
#define ROM_LENGTH		0x1fc

#define	ROM_INFO_LOCATION	(FLOPPY_BOOT_LOCATION+ROM_SEGMENT)
/* at end of floppy boot block */



/* Define a type for passing info to a loaded program */
struct ebinfo {
	uint8_t  major, minor;	/* Version */
	uint16_t flags;		/* Bit flags */
};

/***************************************************************************
External prototypes
***************************************************************************/
extern void rx_qdrain P((void));
extern int tftp P((const char *name, int (*)(unsigned char *, unsigned int, unsigned int, int)));
extern int ip_transmit P((int len, const void *buf));
extern void build_ip_hdr P((unsigned long destip, int ttl, int protocol, 
	int option_len, int len, const void *buf));
extern void build_udp_hdr P((unsigned long destip, 
	unsigned int srcsock, unsigned int destsock, int ttl,
	int len, const void *buf));
extern int udp_transmit P((unsigned long destip, unsigned int srcsock,
	unsigned int destsock, int len, const void *buf));
typedef int (*reply_t)(int ival, void *ptr, unsigned short ptype, struct iphdr *ip, struct udphdr *udp);
extern int await_reply P((reply_t reply,	int ival, void *ptr, long timeout));
extern int decode_rfc1533 P((unsigned char *, unsigned int, unsigned int, int));
void store_ip_in_var(struct in_addr client_ip);
extern void join_group(int slot, unsigned long group);
extern void leave_group(int slot);
//#define RAND_MAX 2147483647L
extern uint16_t ipchksum P((const void *ip, unsigned long len));
extern uint16_t add_ipchksums P((unsigned long offset, uint16_t sum, uint16_t new));
//extern int32_t random P((void));
extern long rfc2131_sleep_interval P((long base, int exp));
extern long rfc1112_sleep_interval P((long base, int exp));
#ifndef DOWNLOAD_PROTO_TFTP
#define	tftp(fname, load_block) 0
#endif
extern void cleanup P((void));

/* misc.c */
extern void twiddle P((void));
extern void sleep P((int secs));
extern void interruptible_sleep P((int secs));
extern void poll_interruptions P((void));
//extern int strcasecmp P((const char *a, const char *b));
//extern char *substr P((const char *a, const char *b));
//extern unsigned long strtoul P((const char *p, const char **, int base));
//extern int printf P((const char *, ...));
//extern int sprintf P((char *, const char *, ...));
//extern int inet_aton P((char *p, struct in_addr *i));
#ifdef PCBIOS
extern void gateA20_set P((void));
#ifdef	RELOCATE
#define gateA20_unset()
#else
extern void gateA20_unset P((void));
#endif
#else
#define gateA20_set()
#define gateA20_unset()
#endif
//extern int putchar P((int));
//extern int getchar P((void));
//extern int iskey P((void));

//extern void grub_printf(const char *, ...);
extern char config_file[128];
//extern void etherboot_printf(const char *,  ...);
//extern int etherboot_sprintf(char *, const char *, ...);
extern int getdec(char **s);
extern void cleanup_net(void);
extern void print_network_configuration (void);
extern int ifconfig (char *, char *, char *, char *);
extern struct arptable_t arptable[MAX_ARP];

#define EnterFunction(func) printf("ENTER: %s\n", func)
#define LeaveFunction(func) printf("LEAVE: %s\n", func)
#undef printf
#undef sprintf
#define printf etherboot_printf
#define sprintf etherboot_sprintf

/*
 * Some codes from etherboot use a level in DEBUG. Define it to be
 * zero means no debug info output, that will make them silence in
 * compiling. Up it as you want.
 */
#ifndef DEBUG
#  define DEBUG 0
#endif

/*#define RPC_DEBUG*/

#endif /* GRUB_H */
