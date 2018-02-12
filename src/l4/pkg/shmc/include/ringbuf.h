/*
 * (c) 2010 Björn Döbel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/shmc/shmc.h>
#include <l4/util/assert.h>
#include <l4/sys/thread.h>

__BEGIN_DECLS

/**
 * \defgroup api_l4shm_ringbuf L4SHM-based ring buffer implementation
 *
 * The library provides a non-locking (strictly 1:1) shared-memory-based ring
 * buffer implementation based on the L4SHM library. It requires an already
 * allocated L4SHM area to be attached to sender and receiver. It will allocate
 * an SHM chunk within this area and provides functions to produce data and
 * consume data in FIFO order from the ring buffer.
 *
 * The sender side of the buffer needs to be initialized *before* the receiver
 * side, because allocation of the SHM chunk and the necessary signals is done
 * on the sender side and the receiver initialization tries to attach to these
 * objects.
 */

/**
 * \defgroup api_l4shm_ringbuf_sender Sender
 * \ingroup api_l4shm_ringbuf
 *
 * \defgroup api_l4shm_ringbuf_receiver Receiver
 * \ingroup api_l4shm_ringbuf
 *
 * \defgroup api_l4shm_ringbuf_internal Internal
 * \ingroup api_l4shm_ringbuf
 */

/*
 * Turn on ringbuf poisoning. This will add magic values to the ringbuf
 * header as well as each packet header and check that these values are
 * valid all the time.
 */
#define L4SHMC_RINGBUF_POISONING 1

/**
 * Head field of a ring buffer.
 *
 * \ingroup l4shmc_ringbuf_internal
 */
typedef struct
{
	volatile l4_uint32_t lock;
	unsigned data_size;
#if L4SHMC_RINGBUF_POISONING
	char     magic1;
#endif
	unsigned next_read;    ///< offset to next read packet
	unsigned next_write;   ///< offset to next write packet
#if L4SHMC_RINGBUF_POISONING
	char     magic2;
#endif
	unsigned bytes_filled; ///< bytes filled in buffer
	unsigned sender_waits; ///< sender waiting?
#if L4SHMC_RINGBUF_POISONING
	char     magic3;
#endif
	char data[];           ///< tail pointer -> data
} l4shmc_ringbuf_head_t;


/**
 * Ring buffer
 *
 * \ingroup l4shmc_ringbuf_internal
 */
typedef struct
{
	l4shmc_area_t   *_area;         ///< L4SHM area this buffer is located in
	l4_cap_idx_t     _owner;        ///< owner (attached to send/recv signal)
	l4shmc_chunk_t   _chunk;        ///< chunk descriptor
	unsigned         _size;         ///< chunk size // XXX do we need this?
	char            *_chunkname;    ///< name of the ring buffer chunk
	char            *_signame;      ///< base name of the ring buffer signals
	l4shmc_ringbuf_head_t  *_addr;         ///< pointer to ring buffer head
	l4shmc_signal_t  _signal_full;  ///< "full" signal - triggered when data is produced
	l4shmc_signal_t  _signal_empty; ///< "empty" signal - triggered when data is consumed
} l4shmc_ringbuf_t;


/**
 * Get ring buffer head pointer.
 *
 * \ingroup l4shmc_ringbuf_internal
 */
#define L4SHMC_RINGBUF_HEAD(ringbuf)       ((l4shmc_ringbuf_head_t*)((ringbuf)->_addr))


/**
 * Get ring buffer data pointer
 *
 * \ingroup l4shmc_ringbuf_internal
 */
#define L4SHMC_RINGBUF_DATA(ringbuf)       (L4SHMC_RINGBUF_HEAD(ringbuf)->data)


/**
 * Get size of data area
 *
 * \ingroup l4shmc_ringbuf_internal
 */
#define L4SHMC_RINGBUF_DATA_SIZE(ringbuf)  ((ringbuf)->_size - sizeof(l4shmc_ringbuf_head_t))

enum lock_content
{
	lock_cont_min = 4,
	locked        = 5,
	unlocked      = 6,
	lock_cont_max = 7,
};

static L4_CV inline void l4shmc_rb_lock(l4shmc_ringbuf_head_t *head)
{
	ASSERT_NOT_NULL(head);
	ASSERT_ASSERT(head->lock > lock_cont_min);
	ASSERT_ASSERT(head->lock < lock_cont_max);

	while (!l4util_cmpxchg32(&head->lock,
	                         unlocked, locked))
		l4_thread_yield();
}


static L4_CV inline void l4shmc_rb_unlock(l4shmc_ringbuf_head_t *head)
{
	ASSERT_NOT_NULL(head);
	ASSERT_ASSERT(head->lock > lock_cont_min);
	ASSERT_ASSERT(head->lock < lock_cont_max);

	head->lock = unlocked;
}

/******************
 * Initialization *
 ******************/

/**
 * Initialize a ring buffer by creating an SHMC chunk and the
 * corresponding signals. This needs to be done by one of the
 * participating parties when setting up communication channel.
 *
 * \pre area has been attached using l4shmc_attach().
 *
 * \param buf           pointer to ring buffer struct
 * \param area          pointer to SHMC area
 * \param chunk_name    name of SHMC chunk to create in area
 * \param signal_name   base name for SHMC signals to create
 * \param size          chunk size
 *
 * \return 0 on success, error otherwise
 *
 */
L4_CV int  l4shmc_rb_init_buffer(l4shmc_ringbuf_t *buf, l4shmc_area_t *area,
                                 char const *chunk_name,
                                 char const *signal_name, unsigned size);

/**
 * De-init a ring buffer.
 */
L4_CV void l4shmc_rb_deinit_buffer(l4shmc_ringbuf_t *buf);



/***************************
 *    RINGBUF SENDER       *
 ***************************/

/**
 * Attach to sender signal of a ring buffer.
 *
 * Attach owner to the sender-side signal of a ring buffer, which
 * is triggered whenever new space has been freed in the buffer for
 * the sender to write to.
 *
 * This is split from initialization, because you may not know the
 * owner cap when initializing the buffer.
 *
 * \param buf      pointer to ring buffer struct
 * \param name     signal base name
 * \param owner    owner thread
 *
 * \return 0 on success, error otherwise
 */
L4_CV int l4shmc_rb_attach_sender(l4shmc_ringbuf_t *buf, char const *,
                                  l4_cap_idx_t owner);


/**
 * Allocate a packet of a given size within the ring buffer.
 *
 * This packet may wrap around at the end of the buffer. Users need
 * to be aware of that.
 *
 * \param psize  packet size
 *
 * \return valid address on success
 * \return             0 if not enough space available
 */
L4_CV char *l4shmc_rb_sender_alloc_packet(l4shmc_ringbuf_head_t *head,
                                          unsigned psize);


/**
 * Copy data into a previously allocated packet.
 *
 * This function is wrap-around aware.
 *
 * \param addr    valid destination (allocate with alloc_packet())
 * \param data    data source
 * \param dsize   data size
 */
L4_CV void l4shmc_rb_sender_put_data(l4shmc_ringbuf_t *buf, char *addr,
                                     char *data, unsigned dsize);


/**
 * Copy in packet from an external data source.
 *
 * This is the function you'll want to use. Just pass it a buffer
 * pointer and let the lib do the work.
 *
 * \param data                valid buffer
 * \param size                data size
 * \param block_if_necessary  bool: block if buffer currently full
 *
 * \return 0           on success
 * \return -L4_ENOMEM  if block == false and no space available
 */
L4_CV int  l4shmc_rb_sender_next_copy_in(l4shmc_ringbuf_t *buf, char *data,
                                          unsigned size, int block_if_necessary);


/**
 * Tell the consumer that new data is available.
 */
L4_CV void l4shmc_rb_sender_commit_packet(l4shmc_ringbuf_t *buf);


/***************************
 *    RINGBUF RECEIVER     *
 ***************************/

/**
 * Initialize receive buffer.
 *
 * Initialize the receiver-side of a ring buffer. This requires the underlying
 * SHMC chunk and the corresponding signals to be valid already (read: to be
 * initialized by the sender).
 *
 * \pre chunk & signals have been created and initialized by the sender side
 *
 * \param buf           pointer to ring buffer struct
 * \param area          pointer to SHMC area
 * \param chunk_name    name of SHMC chunk to create in area
 * \param signal_name   base name for SHMC signals to create
 *
 * \return 0 on success, error otherwise
 *
 */
L4_CV int  l4shmc_rb_init_receiver(l4shmc_ringbuf_t *buf, l4shmc_area_t *area,
                                    char const *chunk_name,
                                    char const *signal_name);


/**
 * Attach to receiver signal of a ring buffer.
 *
 * Attach owner to the receiver-side signal of a ring buffer, which
 * is triggered whenever new data has been produced.
 *
 * This is split from initialization, because you may not know the
 * owner cap when initializing the buffer.
 *
 * \param buf      pointer to ring buffer struct
 * \param name     signal base name
 * \param owner    owner thread
 *
 * \return 0 on success, error otherwise
 */
L4_CV void l4shmc_rb_attach_receiver(l4shmc_ringbuf_t *buf, l4_cap_idx_t owner);


/**
 * Check if (and optionally block until) new data is ready.
 *
 * \param buf      pointer to ring buffer struct
 * \param blocking block if data is not available immediately
 *
 * Returns immediately, if data is available.
 *
 * \return 0 success, data vailable, != 0 otherwise
 */
L4_CV int l4shmc_rb_receiver_wait_for_data(l4shmc_ringbuf_t *buf, int blocking);


/*
 * Copy data out of the buffer.
 *
 * \param target   valid target buffer
 * \param tsize    size of target buffer (must be >= packet size!)
 * \retval tsize   real data size
 * \return 0 on success, negative error otherwise
 */
L4_CV int  l4shmc_rb_receiver_copy_out(l4shmc_ringbuf_head_t *head, char *target,
                                        unsigned *tsize);


/**
 * Notify producer that space is available.
 */
L4_CV void l4shmc_rb_receiver_notify_done(l4shmc_ringbuf_t *buf);


/**
 * Have a look at the ring buffer and see which size the next
 * packet to be read has. Does not modify anything.
 *
 * \return size of next buffer or -1 if no data available
 */
L4_CV int l4shmc_rb_receiver_read_next_size(l4shmc_ringbuf_head_t *head);

__END_DECLS
