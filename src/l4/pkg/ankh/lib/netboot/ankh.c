/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Ankh NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* NIC specific static variables go here */
#include <l4/ankh/client-c.h>
#include <l4/ankh/netboot.h>
#include <stdio.h>
#include <l4/util/assert.h>
#include <arpa/inet.h>

#define STAT_BUF_SIZE 1520

static char *ankh_shm_name;
static int   ankh_bufsize;
static l4_cap_idx_t ankh_rx_cap;
static l4_cap_idx_t ankh_tx_cap;
char ankh_mac[ETH_ALEN];

static char ankh_rx_buf[STAT_BUF_SIZE];
static char ankh_tx_buf[STAT_BUF_SIZE];

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int ankh_poll(struct nic *nic)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */

	unsigned len = sizeof(ankh_rx_buf);
	int err = l4ankh_recv_nonblocking(ankh_rx_buf, &len);

	if (!err) {
		nic->packet = ankh_rx_buf;
		nic->packetlen = len;
	}

	return !err;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void ankh_transmit(
	struct nic *nic,
	const unsigned char *d,	/* Destination */
	unsigned int t,			/* Type */
	unsigned int s,			/* size */
	const char *p)			/* Packet */
{
	memcpy(ankh_tx_buf            , d       , ETH_ALEN);
	memcpy(ankh_tx_buf +  ETH_ALEN, ankh_mac, ETH_ALEN);
	*(unsigned short*)(ankh_tx_buf+ 2*ETH_ALEN) = htons(t);
	memcpy(ankh_tx_buf + ETH_HLEN , p       , s);

	/* send the packet to destination */
	l4ankh_send(ankh_tx_buf, s+ETH_HLEN, 1);
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void ankh_disable(struct dev *dev)
{
	printf("disable\n");
	/* put the card in its initial state */
	/* This function serves 3 purposes.
	 * This disables DMA and interrupts so we don't receive
	 *  unexpected packets or interrupts from the card after
	 *  etherboot has finished. 
	 * This frees resources so etherboot may use
	 *  this driver on another interface
	 * This allows etherboot to reinitialize the interface
	 *  if something is something goes wrong.
	 */
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

int ankh_probe(struct dev* dev);
int ankh_probe(struct dev *dev)
{
	struct nic *nic = (struct nic *)dev;

	/* try to find ankh */
	int ankh_err = l4ankh_open(ankh_shm_name, ankh_bufsize);

	if (!ankh_err)
	{
		int i = 0;
		struct AnkhSessionDescriptor *info = l4ankh_get_info();
		for (i; i < 6; ++i)
		{
			printf("%02X", info->mac[i]);
			if (i < 5) printf(":");
		}
		printf("\n");
//		memcpy(ankh_mac, info->mac, 6);
		memcpy(nic->node_addr, info->mac, 6);

		l4ankh_prepare_recv(ankh_rx_cap);
		l4ankh_prepare_send(ankh_tx_cap);

		/* point to NIC specific routines */
		dev->disable  = ankh_disable;
		nic->poll     = ankh_poll;
		nic->transmit = ankh_transmit;
		return PROBE_WORKED;
	}
	/* else */
	return PROBE_FAILED;
}


void netboot_init(char *shm_name, int bufsize, l4_cap_idx_t rx_cap, l4_cap_idx_t tx_cap)
{
	ankh_shm_name = shm_name;
	ankh_bufsize  = bufsize;
	ankh_rx_cap   = rx_cap;
	ankh_tx_cap   = tx_cap;
}

void twiddle() { }
void poll_interruptions() { }
