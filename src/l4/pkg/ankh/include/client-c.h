#pragma once

#include <l4/sys/compiler.h> // __BEGIN_DECLS
#include <l4/sys/types.h>    // l4_cap_idx_t
#include <l4/shmc/shmc.h>    // l4shmc*()
#include <l4/ankh/session.h> // AnkhSessionDescriptor
#include <l4/shmc/ringbuf.h>

__BEGIN_DECLS

/*
 * Initialize Ankh client library.
 *
 * Call this before everything else.
 */
L4_CV int l4ankh_init(void) L4_NOTHROW;

/*
 * Open SHM connection using shm name and buffer size
 */
L4_CV int l4ankh_open(char *shm_name, int bufsize) L4_NOTHROW;

/*
 * Get the connection's info area.
 */
L4_CV struct AnkhSessionDescriptor *l4ankh_get_info(void) L4_NOTHROW;

/*************************
 * Send()
 *************************/

/*
 * Prepare to send data.
 *
 * Attaches to the underlying ringbuf's send signal.
 * Call before sending any data.
 */
L4_CV int l4ankh_prepare_send(l4_cap_idx_t owner) L4_NOTHROW;

/*
 * Send packet giving buffer and size.
 *
 * \param data	buffer
 * \param size	buffer size
 * \param block	block if buffer full
 */
L4_CV int l4ankh_send(char *data, unsigned size, char block) L4_NOTHROW;


/*************************
 * Receive
 *************************/

/*
 * Prepare packet reception.
 *
 * Attaches to the underlying ringbuf's recv signal.
 * Call before receiving any data.
 */
L4_CV int l4ankh_prepare_recv(l4_cap_idx_t owner) L4_NOTHROW;

// XXX: interface - send() has a block parameter, recv() has
//      two different versions -- FIX
/*
 * Receive data, block if buffer empty.
 *
 * Note: size is an in/out parameter. Specifies the maximum space available in
 *       the buffer upon call. Returns the used size.
 */
L4_CV int l4ankh_recv_blocking(char *buffer, unsigned *size) L4_NOTHROW;

/*
 * Receive data, return error if none available
 *
 * Note: size is an in/out parameter. Specifies the maximum space available in
 *       the buffer upon call. Returns the used size.
 */
L4_CV int l4ankh_recv_nonblocking(char *buffer, unsigned *size) L4_NOTHROW;

/*
 * Get access to the underlying send buffer.
 *
 * This can be used to access the underlying buffer and perform
 * more sophisticated operations that are needed only by special
 * clients (L4Linux, ...).
 */
L4_CV l4shmc_ringbuf_t *l4ankh_get_sendbuf(void) L4_NOTHROW;

/*
 * Get access to the underlying recv buffer.
 *
 * This can be used to access the underlying buffer and perform
 * more sophisticated operations that are needed only by special
 * clients (L4Linux, ...).
 */
L4_CV l4shmc_ringbuf_t *l4ankh_get_recvbuf(void) L4_NOTHROW;

__END_DECLS
