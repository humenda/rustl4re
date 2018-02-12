 /*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#ifndef	NIC_H
#define NIC_H

#include "dev.h"

/* The 'rom_info' maybe arch depended. It must be moved to some other
 * place */
struct rom_info {
	unsigned short	rom_segment;
	unsigned short	rom_length;
};

/*
 *	Structure returned from eth_probe and passed to other driver
 *	functions.
 */
struct nic
{
	struct dev	dev;  /* This must come first */
	int		(*poll)P((struct nic *));
	void		(*transmit)P((struct nic *, const unsigned char *d,
				unsigned int t, unsigned int s, const char *p));
	int		flags;	/* driver specific flags */
	struct rom_info	*rom_info;	/* -> rom_info from main */
	unsigned char	*node_addr;
	unsigned char	*packet;
	unsigned int	packetlen;
	void		*priv_data;	/* driver can hang private data here */
};

extern unsigned char *hostname;

extern int hostnamelen;

/* Current Network Interface Card */
extern struct nic nic;

/* Whether network is ready */
extern int network_ready;

/* User aborted in await_reply if not zero */
extern int user_abort;

/** 
 * Some network functions.
 **/
extern int  eth_probe(void);

extern int  eth_poll(void);

extern void eth_transmit(const unsigned char * __d, unsigned int __t, 
			 unsigned int __s, const void * __p);

extern void eth_disable(void);



extern int rarp(void);

extern int bootp(void);

extern int dhcp(void);

#endif	/* NIC_H */
