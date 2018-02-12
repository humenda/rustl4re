#ifndef __DDE_26_NET_H
#define __DDE_26_NET_H

#include <l4/sys/compiler.h>
#include <linux/skbuff.h>

__BEGIN_DECLS

/** rx callback function */
typedef int (*linux_rx_callback)(struct sk_buff *);

extern linux_rx_callback l4dde26_rx_callback;

/** Register rx callback function.
 *
 * This registers a function to be a rx callback. Whenever an ethernet packet
 * arrives and is processed by a driver or a softirq, it will end up in either
 * netif_rx() or netif_receive_skb(). Both will finally try to hand this to
 * a DDE user using a previously registered callback.
 *
 * \param cb   new callback function
 * \return     old callback function pointer
 */
linux_rx_callback l4dde26_register_rx_callback(linux_rx_callback cb);


/** Run callback function.
 */
static inline int l4dde26_do_rx_callback(struct sk_buff *s)
{
	if (l4dde26_rx_callback != NULL)
		return l4dde26_rx_callback(s);

	return 0;
}

__END_DECLS

#endif
