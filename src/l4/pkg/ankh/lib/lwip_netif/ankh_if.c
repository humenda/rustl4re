/**
 * @file
 * Ankh Interface driver
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ankhif" to replace it with
 * something that better describes your network interface.
 */
#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/dhcp.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"
#include <lwip-util.h>

#include <l4/ankh/client-c.h>
#include <l4/ankh/lwip-ankh.h>
#include <l4/util/util.h>
#include <assert.h>
#include <stdlib.h>


/* Define those to better describe your network interface. */
#define IFNAME0 'a'
#define IFNAME1 'n'

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ankhif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
  ankh_config_info *config;
  pthread_t         recv_thread;
};

/* Forward declarations. */
static void  ankhif_input(struct netif *netif);

static void *ankhif_recv_fn(void *arg)
{
    int err;
    struct netif *netif = (struct netif*)arg;
    l4shmc_ringbuf_t *rb = l4ankh_get_recvbuf();

    struct ankhif *ankhif = netif->state;
    ankhif->config->recv_thread = pthread_l4_cap(pthread_self());
    err = l4ankh_prepare_recv(ankhif->config->recv_thread);

    printf("ANKHIF::Recv rb @ %p\n", rb);

    for (;;)
    {
        err = l4shmc_rb_receiver_wait_for_data(rb, 1);
		if (err) {
			continue;
		}
		ankhif_input(netif);
    }

    return NULL;
}

/**
 * In this function, the hardware should be initialized.
 * Called from ankhif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ankhif
 */
static void
low_level_init(struct netif *netif)
{
  struct ankhif *ankhif = netif->state;
  short i;

  int err = l4ankh_open(ankhif->config->shm_name, ankhif->config->bufsize);
  assert(!err);

  err = l4ankh_prepare_send(ankhif->config->send_thread);
  assert(!err);

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  memcpy(netif->hwaddr, l4ankh_get_info()->mac, ETHARP_HWADDR_LEN);
  printf("--> ");
  for (i=0; i < ETHARP_HWADDR_LEN; ++i) {
      printf("%02X", netif->hwaddr[i]);
      if (i < 5) printf(":");
  }
  printf(" <--\n");

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  /* Do whatever else is needed to initialize interface. */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ankhif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  struct ankhif *ankhif __attribute__((unused)) = netif->state;
  struct pbuf *q = NULL;

// XXX  initiate transfer();

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

#if 0
  if (p) {
    printf("%s, %p %d ... ", __func__, p->payload, p->len);
    __hexdump(p->payload, p->len < 12 ? p->len : 12);
  }
  else {
    printf("send with NULL workload?\n");
  }
#endif

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
      int x = l4ankh_send(q->payload, q->len, 1);
      if (x)
          printf("  ankh_send: %d\n", x);
  }

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ankhif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif)
{
  struct ankhif *ankhif __attribute__((unused)) = netif->state;
  struct pbuf *p;
  int len;
  l4shmc_ringbuf_t *rb = l4ankh_get_recvbuf();

  /* Obtain the size of the packet and put it into the "len"
     variable. */
  len = l4shmc_rb_receiver_read_next_size(L4SHMC_RINGBUF_HEAD(rb));
  if (len <= 0)
      return NULL;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* we allocate a buffer for the whole packet */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);

  if (p != NULL) {

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  /* Read enough bytes to fill this pbuf in the chain. The
   * available data in the pbuf is given by the p->len
   * variable. */
    assert(p->len == len);
    int err = l4shmc_rb_receiver_copy_out(L4SHMC_RINGBUF_HEAD(rb), p->payload, (unsigned*)&p->len);
#if 0
    printf("Incoming %p: ", p); __hexdump(p->payload, 12);
#endif
    if (err) { /* Should not happen! */
      printf("ERROR: l4shmc_rb_copy_out %d\n", err);
      printf("p @ %p, len %d\n", p, p->len);
      return NULL;
    }

#if ETH_PAD_SIZE
      pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

      LINK_STATS_INC(link.recv);
  } else {
    // XXX drop packet();
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  return p;
}

static inline char *type_to_string(unsigned type)
{
    switch(type) {
        case ETHTYPE_IP:
            return "IP";
        case ETHTYPE_ARP:
            return "ARP";
        case ETHTYPE_PPPOE:
            return "PPPoE";
    }
    return "unknown";
}


/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ankhif
 */
static void
ankhif_input(struct netif *netif)
{
  struct ankhif *ankhif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;
  int x;

  ankhif = netif->state;

  /* move received packet into a new pbuf */
  p = low_level_input(netif);
  /* no packet could be read, silently ignore this */
  if (p == NULL) return;
  /* points to packet payload, which starts with an Ethernet header */
  ethhdr = p->payload;

  switch (htons(ethhdr->type)) {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
  /* PPPoE packet? */
  case ETHTYPE_PPPOEDISC:
  case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    /* full packet send to tcpip_thread to process */
	x = netif->input(p, netif);
    if (x!=ERR_OK)
     { LWIP_DEBUGF(NETIF_DEBUG, ("ankhif_input: IP input error\n"));
       pbuf_free(p);
       p = NULL;
     }
    break;

  default:
	printf("unknown ethtype %lx\n", htons(ethhdr->type));
    pbuf_free(p);
    p = NULL;
    break;
  }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ankhif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ankhif_init(struct netif *netif);
err_t ankhif_init(struct netif *netif)
{
  struct ankhif *ankhif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ankhif = mem_malloc(sizeof(struct ankhif));
  if (ankhif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("ankhif_init: out of memory\n"));
    return ERR_MEM;
  }
  ankhif->config = netif->state;

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000);

  netif->state = ankhif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  ankhif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

  /* initialize the hardware */
  low_level_init(netif);

  int err = pthread_create(&ankhif->recv_thread, NULL, ankhif_recv_fn, netif);
  assert(!err);

  return ERR_OK;
}

/******************************
 * Client initialization code *
 ******************************/
#define DEBUG_L4LWIP_INIT 0

// default configuration
static ankh_config_info cfg = { 2048, L4_INVALID_CAP,
                                L4_INVALID_CAP, "" };
// default interface
struct netif ankh_netif;
static ip_addr_t ipaddr, netmask, gw;

static int try_l4re_option(int argc, char **argv, unsigned idx)
{
    int remove = 0;

    if (strcmp(argv[idx], "--shm") == 0) {
        snprintf(cfg.shm_name, CFG_SHM_NAME_SIZE, "%s", argv[idx+1]);
        remove = 2;
    }
    if (strcmp(argv[idx], "--bufsize") == 0) {
        cfg.bufsize = atoi(argv[idx+1]);
        remove = 2;
    }

    memcpy(&argv[idx], &argv[idx+remove], (argc-idx-remove) * sizeof(char*));
    return remove;
}


int l4_lwip_init(int *argc, char **argv)
{
    int i = 0;

#if DEBUG_L4LWIP_INIT
    printf("Arguments: %d\n", *argc);
    printf("Command line: \n");
#endif

    while (i < *argc) {
#if DEBUG_L4LWIP_INIT
        printf("  %d %s\n", i, argv[i]);
#endif
        unsigned removed = try_l4re_option(*argc, argv, i);
        if (removed)
            *argc -= removed;
        else
            ++i;
    }
#if DEBUG_L4LWIP_INIT
    printf("parsed args. argc is now: %d\n", *argc);
    printf("Initializing Ankh\n");
#endif

    cfg.send_thread =  pthread_l4_cap(pthread_self());
    l4ankh_init();

    tcpip_init(NULL, NULL);

	memset(&ankh_netif, 0, sizeof(struct netif));
    netif_add(&ankh_netif, &ipaddr, &netmask, &gw, &cfg,
              ankhif_init, ethernet_input);
    netif_set_default(&ankh_netif);

#if DEBUG_L4LWIP_INIT
    printf("dhcp start...\n");
#endif
    dhcp_start(&ankh_netif);
    while(!netif_is_up(&ankh_netif)) {
#if DEBUG_L4LWIP_INIT
        //printf("dhcp pending\n");
#endif
        l4_sleep(1000);
    }
    printf("IP: "); lwip_util_print_ip(&ankh_netif.ip_addr); printf("\n");
    printf("MASK: "); lwip_util_print_ip(&ankh_netif.netmask); printf("\n");
    printf("GW: "); lwip_util_print_ip(&ankh_netif.gw); printf("\n");

    return 0;
}
