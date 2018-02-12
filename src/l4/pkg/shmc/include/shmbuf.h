/*
 * Copyright (c) 2011 Stefan Fritsch <stefan_fritsch@genua.de>
 *                    Christian Ehrhardt <christian_ehrhardt@genua.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <l4/shmc/shmc.h>
#include <l4/sys/err.h>
#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/* sizeof(struct l4shm_buf_pkt_head) must be power of two */
struct l4shm_buf_pkt_head
{
  unsigned long size;
};

struct l4shm_buf_chunk_head
{
  /** end of ring content */
  volatile unsigned long next_offs_to_write;
  /** start of ring content */
  volatile unsigned long next_offs_to_read;
  /** ring buffer full */
  volatile unsigned long writer_blocked;
  /** The packet buffers. */
  volatile struct l4shm_buf_pkt_head pkg[0];
};

/**
 * Return the size of the largest packet that currently fits into
 * the given chunk.
 * @param chunk The chunk.
 * @param chunksize The size of the data area of the chunk (maintained
 *     outside of the shared memory area).
 * It is the callers responsibility to ensure that it is the sender of
 * this chunk.
 */
L4_INLINE unsigned long l4shm_buf_chunk_tx_free(struct l4shm_buf_chunk_head *chunk,
    unsigned long chunksize)
{
  unsigned long roff = chunk->next_offs_to_read;
  unsigned long woff = chunk->next_offs_to_write;
  unsigned long space;

  if (woff >= chunksize || roff >= chunksize)
    return 0;

  if (woff < roff)
    space = roff - woff;
  else
    space = chunksize - (woff - roff);
  if (space < 2 * sizeof(struct l4shm_buf_pkt_head))
    return 0;
  return space - 2 * sizeof(struct l4shm_buf_pkt_head);
}

/** align v to power of 2 boundary */
L4_INLINE l4_umword_t l4shmc_align(l4_umword_t v, l4_umword_t boundary)
{
  return (v + boundary - 1 ) & ~(boundary - 1);
}

L4_INLINE volatile struct l4shm_buf_pkt_head *l4shmc_align_ptr_ph(volatile struct l4shm_buf_pkt_head *v)
{
  return (struct l4shm_buf_pkt_head *)l4shmc_align((l4_umword_t)v, sizeof(struct l4shm_buf_pkt_head));
}

L4_INLINE l4_umword_t l4shmc_align_off_ph(l4_umword_t v)
{
  return l4shmc_align(v, sizeof(struct l4shm_buf_pkt_head));
}

/**
 * Initialize a chunk.
 * Note that the entire chunk structure lives in shared memory.
 * @param chunk The chunk structure.
 * @param chunksize The size of the chunk including the l4shm_buf_chunk_head
 *     structure. This value is not maintained inside the chunk head
 *     but used to check alignment requirements.
 * @return True if initialization was successful, a negative error code
 *     if initialization failed.
 */
L4_INLINE int l4shm_buf_chunk_init(struct l4shm_buf_chunk_head *chunk,
				   unsigned long chunksize)
{
  static_assert(0 == ( sizeof(struct l4shm_buf_pkt_head) &
		       (sizeof(struct l4shm_buf_pkt_head)-1) ),
		"sizeof(struct l4shm_buf_pkt_head) not power of 2");

  if (chunk->pkg != l4shmc_align_ptr_ph(chunk->pkg))
    return -L4_EINVAL;
  chunksize -= sizeof(struct l4shm_buf_chunk_head);
  if (chunksize != l4shmc_align_off_ph(chunksize))
    return -L4_EINVAL;
  chunk->pkg[0].size = 0;
  chunk->next_offs_to_write = 0,
	 chunk->next_offs_to_read = 0;
  chunk->writer_blocked = 0;
  return 0;
}

/**
 * Add part of a packet to the given chunk. The data is only copied
 * into the area of the chunk reserved for the sender. It is not made
 * available for the receiver. Use l4shm_buf_chunk_tx_complete for this.
 * It is the callers responsibility to ensure that it is the sender
 * on this chunk.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @param buf The packet data.
 * @param poffset The offset of this chunk within the packet. The caller must
 *     make sure that it only commits complete packets.
 * @param len The length of the packet data.
 * @return Zero if the packet was added to the chunk, a negative error
 *     code otherwise. In particular -L4_EAGAIN means that insufficient
 *     space was available in the ring.
 */
L4_INLINE int l4shm_buf_chunk_tx_part(struct l4shm_buf_chunk_head *ch,
				      unsigned long chunksize, const void *buf,
				      unsigned long poffset, unsigned long len)
{
  unsigned long woffset, nextoffset, r, part_len, totallen;
  int blocked = 0;

  if (len == 0)
    return 0;
  if (poffset > chunksize || len > chunksize)
    return -L4_ENOMEM;
  totallen = poffset + len;
  if (totallen > chunksize - 2*sizeof(struct l4shm_buf_pkt_head))
    return -L4_ENOMEM;

retry:
  __sync_synchronize();
  woffset = ch->next_offs_to_write;
  if (woffset >= chunksize || woffset != l4shmc_align_off_ph(woffset))
    return -L4_EIO;

  nextoffset = l4shmc_align_off_ph(woffset + totallen
				   + sizeof(struct l4shm_buf_pkt_head));

  r = ch->next_offs_to_read;
  if (r >= chunksize)
    return -L4_EIO;
  if (r <= woffset)
    r += chunksize;

  /* Don't use all space, L4Linux needs an additional '0' chunk head after
   * the chunk. Therefore we need an additional struct l4shm_buf_pkt_head.
   */
  if (nextoffset + sizeof(struct l4shm_buf_pkt_head) > r)
    {
      /*
       * If there is insufficient space set writer_blocked and
       * retry. This is neccessary to avoid a race where we set
       * writer blocked after the peer emptied the buffer. We don't
       * set writer_blocked in the first try to avoid spurious interrupts
       * triggered by the reader due to writer_blocked.
       */
      if (blocked)
	return  -L4_EAGAIN;
      ch->writer_blocked = 1;
      blocked = 1;
      goto retry;
    }

  ch->writer_blocked = 0;

  woffset += sizeof(struct l4shm_buf_pkt_head) + poffset;
  woffset %= chunksize;

  if (woffset + len > chunksize)
    part_len = chunksize - woffset;
  else
    part_len = len;

  memcpy(((unsigned char *)ch->pkg)+woffset, buf, part_len);
  if (len != part_len)
    memcpy((void*)ch->pkg, (char const *)buf + part_len, len - part_len);

  return 0;
}

/**
 * Complete the transmission of a packet. This function fills in the
 * packet head in the shm buffer. The packet data must already be present.
 * Use l4shm_buf_chunk_tx_part to copy packet data.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @param pkglen The total length of the packet.
 * @return Zero in case of success or a negative error code.
 */
L4_INLINE int l4shm_buf_chunk_tx_complete(struct l4shm_buf_chunk_head *ch,
    unsigned long chunksize, size_t pkglen)
{
  unsigned long offset, nextoffset, r;
  volatile struct l4shm_buf_pkt_head *ph, *next_ph;

  if (pkglen == 0)
    return 0;
  if (pkglen > chunksize - 2*sizeof(struct l4shm_buf_pkt_head))
    return -L4_ENOMEM;

  offset = ch->next_offs_to_write;
  if (offset >= chunksize || offset != l4shmc_align_off_ph(offset))
    return -L4_EIO;
  ph = ch->pkg + (offset / sizeof(struct l4shm_buf_pkt_head));

  nextoffset = l4shmc_align_off_ph(offset + pkglen + sizeof(struct l4shm_buf_pkt_head));

  r = ch->next_offs_to_read;
  if (r >= chunksize)
    return -L4_EIO;
  if (r <= offset)
    r += chunksize;

  /* Don't use all space, L4Linux needs an additional '0' chunk head after
   * the chunk. Therefore we need an additional struct l4shm_buf_pkt_head.
   */
  if (nextoffset + sizeof(struct l4shm_buf_pkt_head) > r)
    return  -L4_EIO;

  nextoffset %= chunksize;
  /* For L4Linux compatibility */
  next_ph = ch->pkg + (nextoffset / sizeof(struct l4shm_buf_pkt_head));
  next_ph->size = 0;

  __sync_synchronize();

  ph->size = pkglen;
  ch->next_offs_to_write = nextoffset;
  return 0;
}

/**
 * Add a packet to the given chunk.
 * It is the callers responsibility to ensure that it is the sender
 * on this chunk.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @param buf The packet data.
 * @param size The length of the packet data.
 * @return Zero if the packet was added to the chunk, a negative error
 *     code otherwise. In particular -L4_EAGAIN means that insufficient
 *     space was available in the ring.
 * It is the callers responsibility to wake up the receiver after adding
 * packets or when detecting insufficient space in the buffer.
 */
L4_INLINE int l4shm_buf_chunk_tx(struct l4shm_buf_chunk_head *ch,
				 unsigned long chunksize,
				 const void *buf, unsigned long pkt_size)
{
  int ret = l4shm_buf_chunk_tx_part(ch, chunksize, buf, 0, pkt_size);
  if (ret < 0)
    return ret;
  return l4shm_buf_chunk_tx_complete(ch, chunksize, pkt_size);
}


/**
 * Return the length of the next packet in the chunk.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *    excluding the leading l4shm_buf_chunk_head structure.
 * @return Zero if the chunk is empty, the length of the first
 *    packet in the chunk or a negative value in case of an error.
 */
L4_INLINE int l4shm_buf_chunk_rx_len(struct l4shm_buf_chunk_head *ch,
				     unsigned long chunksize)
{
  unsigned long offset = ch->next_offs_to_read;
  unsigned long woffset = ch->next_offs_to_write;
  long space = woffset - offset;
  unsigned long pkt_size;
  volatile struct l4shm_buf_pkt_head *ph;

  if (offset >= chunksize)
    return -L4_EIO;
  if (woffset >= chunksize)
    return -L4_EIO;
  if (space == 0)
    return 0;
  if (space < 0)
    space += chunksize;
  if (space < (int)sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  ph = ch->pkg + (offset / sizeof(struct l4shm_buf_pkt_head));
  pkt_size = ph->size;
  if (pkt_size > (unsigned long)space - sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  return pkt_size;
}

/**
 * Drop the first packet in the chunk. The packet must have non-zero length.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @return Zero if the packet could be dropped, a negative value in case
 *     of an error.
 */
L4_INLINE int l4shm_buf_chunk_rx_drop(struct l4shm_buf_chunk_head *ch,
				      unsigned long chunksize)
{
  unsigned long offset = ch->next_offs_to_read;
  unsigned long woffset = ch->next_offs_to_write;
  long space = woffset - offset;
  unsigned long pkt_size;
  volatile struct l4shm_buf_pkt_head *ph;

  if (offset >= chunksize)
    return -L4_EIO;
  if (woffset >= chunksize)
    return -L4_EIO;
  if (space == 0)
    return -L4_ENOENT;
  if (space < 0)
    space += chunksize;
  if (space < (int)sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  ph = ch->pkg + (offset / sizeof(struct l4shm_buf_pkt_head));
  pkt_size = ph->size;
  if (pkt_size > (unsigned long)space - sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  offset = l4shmc_align_off_ph(offset + sizeof(struct l4shm_buf_pkt_head) + pkt_size);
  offset %= chunksize;
  ch->next_offs_to_read = offset;
  __sync_synchronize();

  return 0;
}

/**
 * Copy part of a packet from the shm chunk to a buffer. The caller
 * must make sure that the packet contains enough data and that the
 * buffer is long enough to receive the data.
 * @param ch The chunk. The data in the chunk is not modified!
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @param poffset The offset of the part to copy within the packet.
 *     It is an error if this offset is beyond the length of the packet.
 * @param len The amount to copy. It is an error if the packet is shorter
 *     than offset+len bytes.
 * @param buf The target buffer. The caller must make sure that the buffer
 *     can hold len bytes.
 * @return Zero or a negative error code.
 */
L4_INLINE int l4shm_buf_chunk_rx_part(const struct l4shm_buf_chunk_head *ch,
				      unsigned long chunksize,
				      unsigned long poffset,
				      unsigned long len, void *buf)
{
  unsigned long roffset = ch->next_offs_to_read;
  unsigned long woffset = ch->next_offs_to_write;
  long space = woffset - roffset;
  volatile const struct l4shm_buf_pkt_head *ph;
  unsigned long part_len, pkt_size;

  if (roffset >= chunksize)
    return -L4_EIO;
  if (woffset >= chunksize)
    return -L4_EIO;
  if (space < 0)
    space += chunksize;
  if (space < (int)sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  ph = ch->pkg + (roffset / sizeof(struct l4shm_buf_pkt_head));
  pkt_size = ph->size;
  if (pkt_size > (unsigned long)space - sizeof(struct l4shm_buf_pkt_head))
    return -L4_EIO;
  /* Backward compatibility (pkt_size == 0 means no more packets) */
  if (pkt_size == 0)
    return -L4_ENOENT;
  if (poffset >= pkt_size || len > pkt_size || poffset + len > pkt_size)
    return -L4_ENOENT;
  roffset += poffset + sizeof(struct l4shm_buf_pkt_head);
  roffset %= chunksize;

  if (roffset + len > chunksize)
    part_len = chunksize - roffset;
  else
    part_len = len;
  memcpy(buf, ((unsigned char *)ch->pkg)+roffset, part_len);
  if (part_len != len)
    memcpy((char*)buf + part_len, (unsigned char *)ch->pkg, len - part_len);

  return 0;
}

/**
 * Remove a packet from the given chunk.
 * It is the callers responsibility to ensure that it is the receiver
 * on this chunk.
 * @param ch The chunk.
 * @param chunksize The size of the payload data in the chunk, i.e.
 *     excluding the leading l4shm_buf_chunk_head structure.
 * @param buf The packet buffer.
 * @param size The length of the packet buffer.
 * @return The size of the received packet, zero if no packet was available
 *    and a negative error code otherwise.
 * It is the callers responsibility to wake up a potentially blocked
 * sender after removing data from the buffer.
 */
L4_INLINE int l4shm_buf_chunk_rx(struct l4shm_buf_chunk_head *ch,
				 unsigned long chunksize,
				 void *buf, unsigned long buf_size)
{
  int ret = l4shm_buf_chunk_rx_len(ch, chunksize);
  if (ret <= 0)
    return ret;
  if ((int)buf_size < ret)
    return -L4_ENOMEM;
  ret = l4shm_buf_chunk_rx_part(ch, chunksize, 0, ret, buf);
  if (ret < 0)
    return ret;
  return l4shm_buf_chunk_rx_drop(ch, chunksize);
}

/*
 * Various wrappers for l4shm_buf_chunk_* functions using a single l4shm_buf struct
 */

struct l4shm_buf
{
  struct l4shm_buf_chunk_head *tx_head;
  struct l4shm_buf_chunk_head *rx_head;
  unsigned long tx_ring_size;
  unsigned long rx_ring_size;
};

/**
 * Initialize an l4shm_buf structure with pre-allocated Rx/Tx rings
 * @param sb A pre-allocated l4shm_buf structure to  initialize.
 * @param rx_chunk The receive ring for this thread.
 * @param rx_size The size of the receive ring including the l4shm_buf_chunk_head.
 * @param tx_chunk The transmit ring for this thread.
 * @param tx_size The size of the transmit ring including the l4shm_buf_chunk_head.
 * @return True if successful, a negative error code if initialization
 *     failed (normally due to bad alignment).
 * This library will ensure that this thread only adds data to the tx
 * ring and only removes data from the rx ring.
 * It is possible to do initialization in two steps, by calling l4shm_buf_init()
 * twice, once with rx_chunk == NULL and once with tx_chunk == NULL.
 */
L4_INLINE int l4shm_buf_init(struct l4shm_buf *sb, void *rx_chunk, unsigned long rx_size,
			     void *tx_chunk, unsigned long tx_size)
{
  if (tx_chunk)
    {
      if (l4shm_buf_chunk_init((struct l4shm_buf_chunk_head *)tx_chunk, tx_size))
	return -L4_EINVAL;
      sb->tx_head = (struct l4shm_buf_chunk_head *)tx_chunk;
      sb->tx_ring_size = tx_size - sizeof(struct l4shm_buf_chunk_head);
    }
  else
    {
      sb->tx_head = NULL;
    }

  if (rx_chunk)
    {
      if (l4shm_buf_chunk_init((struct l4shm_buf_chunk_head *)rx_chunk, rx_size))
	return -L4_EINVAL;
      sb->rx_head = (struct l4shm_buf_chunk_head *)rx_chunk;
      sb->rx_ring_size = rx_size - sizeof(struct l4shm_buf_chunk_head);
    }
  else if (!tx_chunk)
    {
      return -L4_EINVAL;
    }
  else
    {
      sb->rx_head = NULL;
    }

  return 0;
}

L4_INLINE int l4shm_buf_rx_len(struct l4shm_buf *sb)
{
  return l4shm_buf_chunk_rx_len(sb->rx_head, sb->rx_ring_size);
}

L4_INLINE int l4shm_buf_rx_drop(struct l4shm_buf *sb)
{
  return l4shm_buf_chunk_rx_drop(sb->rx_head, sb->rx_ring_size);
}

L4_INLINE int l4shm_buf_rx_part(struct l4shm_buf *sb, unsigned long poffset,
				unsigned long len, char *buf)
{
  return l4shm_buf_chunk_rx_part(sb->rx_head, sb->rx_ring_size, poffset, len, buf);
}

L4_INLINE unsigned long l4shm_buf_tx_free(struct l4shm_buf *sb)
{
  return l4shm_buf_chunk_tx_free(sb->tx_head, sb->tx_ring_size);
}

L4_INLINE int l4shm_buf_tx(struct l4shm_buf *sb, const void *buf,
			   unsigned long pkt_size)
{
  return l4shm_buf_chunk_tx(sb->tx_head, sb->tx_ring_size, buf, pkt_size);
}

L4_INLINE int l4shm_buf_rx(struct l4shm_buf *sb, void *buf,
			   unsigned long buf_size)
{
  return l4shm_buf_chunk_rx(sb->rx_head, sb->rx_ring_size, buf, buf_size);
}


L4_INLINE int l4shm_buf_tx_part(struct l4shm_buf *sb, const char *buf,
				unsigned long poffset, unsigned long len)
{
  return l4shm_buf_chunk_tx_part(sb->tx_head, sb->tx_ring_size, buf, poffset, len);
}

L4_INLINE int l4shm_buf_tx_complete(struct l4shm_buf *sb, size_t pktlen)
{
  return l4shm_buf_chunk_tx_complete(sb->tx_head, sb->tx_ring_size, pktlen);
}

EXTERN_C_END

