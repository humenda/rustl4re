#ifndef	_UDP_H
#define	_UDP_H

/* We need 'uint16_t' and 'uint8_t'  */
#include "types.h"
/* We need 'struct in_addr' */
#include <netinet/in.h>

struct udp_pseudo_hdr {
	struct in_addr  src;
	struct in_addr  dest;
	uint8_t  unused;
	uint8_t  protocol;
	uint16_t len;
};
struct udphdr {
	uint16_t src;
	uint16_t dest;
	uint16_t len;
	uint16_t chksum;
};

extern void build_udp_hdr(unsigned long __destip, unsigned int __srcsock, 
	      unsigned int __destsock, int __ttl, int __len, 
	      const void * __buf);

extern int udp_transmit(unsigned long __destip, unsigned int __srcsock,
			unsigned int __destsock, int __len, const void * __buf);

#endif	/* _UDP_H */
