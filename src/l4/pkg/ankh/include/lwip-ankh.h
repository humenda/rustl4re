#pragma once

#include "lwip/tcpip.h"

enum
{
	CFG_SHM_NAME_SIZE = 30,
};

typedef struct
{
	unsigned bufsize;
	l4_cap_idx_t send_thread;
	l4_cap_idx_t recv_thread;
	char shm_name[CFG_SHM_NAME_SIZE];
} ankh_config_info;


L4_INLINE void __hexdump(unsigned char *buf, unsigned len)
{
	unsigned i = 0;

	for ( ; i < len; ++i)
		printf("%02x ", *(buf + i));
	printf("\n");
}

/*
 * Initialize an LWIP client up to the point where it becomes
 * ready to send/receive. This includes
 * 1) Ankh initialization
 * 2) Setting up the LWIP network device
 * 3) performing DHCP to obtain an IP
 *
 * Ankh arguments are extracted from argc and argv. Both are modified
 * accordingly, so that it does not contain these args afterwards.
 */
int l4_lwip_init(int *argc, char **argv);

extern struct netif ankh_netif;
