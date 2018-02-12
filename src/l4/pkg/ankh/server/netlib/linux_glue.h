#pragma once

#include <l4/sys/compiler.h>

__BEGIN_DECLS

#define mac_fmt       "%02X:%02X:%02X:%02X:%02X:%02X"
#define mac_str(mac)  (unsigned char)((mac)[0]), (unsigned char)((mac)[1]), \
                      (unsigned char)((mac)[2]), (unsigned char)((mac)[3]), \
                      (unsigned char)((mac)[4]), (unsigned char)((mac)[5])
/*
 * Linux-style fn to loop through all network devices and perform
 * necessary setup. Called during Ankh initialization.
 */
int open_network_devices(int);
void ankh_set_debug(void);
void ankh_unset_debug(void);

/*
 * Interface functions to be used by higher-level C++ code.
 */
int netdev_irq(void *ptr);
char const *netdev_name(void *ptr);
int netdev_set_promisc(void *ptr);
int netdev_unset_promisc(void *ptr);
int netdev_get_promisc(void *ptr);
char *netdev_dev_addr(void *ptr);
int netdev_mtu(void *ptr);

/*
 * Function to register a network device with the upper layer.
 */
void netdev_add(void *ptr);

/*
 * Allocate a buffer that can be used for DMA using lower-level
 * functions.
 */
void *alloc_dmaable_buffer(unsigned size);

/*
 * Hand a packet upwards
 *
 * \param packet    pointer to packet
 * \param len       packet data size
 *
 * \return <num>    number of clients this packet was delivered to
 */
int packet_deliver(void *packet, unsigned len, char const * const name, unsigned local);

/*
 * Transmit packet
 */
int netdev_xmit(void *ptr, char *addr, unsigned len);

/*
 * Enable UX-style Linux system calls for the calling thread.
 */
void enable_ux_self(void);

__END_DECLS
