/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
/**
 * \internal
 * \file
 */
#pragma once

#ifndef __INCLUDED_FROM_L4SHMC_H__
#error Do not include l4/shm/internal.h directly, use l4/shm/shm.h!
#endif

#include <l4/sys/irq.h>
#include <l4/util/atomic.h>

L4_CV L4_INLINE long
l4shmc_attach(const char *shmc_name, l4shmc_area_t *shmarea)
{
  return l4shmc_attach_to(shmc_name, 0, shmarea);
}

L4_CV L4_INLINE long
l4shmc_wait_any(l4shmc_signal_t **p)
{
  return l4shmc_wait_any_to(L4_IPC_NEVER, p);
}

L4_CV L4_INLINE long
l4shmc_wait_any_try(l4shmc_signal_t **p)
{
  return l4shmc_wait_any_to(L4_IPC_BOTH_TIMEOUT_0, p);
}

L4_CV L4_INLINE long
l4shmc_wait_signal_try(l4shmc_signal_t *s)
{
  return l4shmc_wait_signal_to(s, L4_IPC_BOTH_TIMEOUT_0);
}

L4_CV L4_INLINE long
l4shmc_wait_signal(l4shmc_signal_t *s)
{
  return l4shmc_wait_signal_to(s, L4_IPC_NEVER);
}

L4_CV L4_INLINE long
l4shmc_wait_chunk_try(l4shmc_chunk_t *p)
{
  return l4shmc_wait_chunk_to(p, L4_IPC_BOTH_TIMEOUT_0);
}

L4_CV L4_INLINE long
l4shmc_wait_chunk(l4shmc_chunk_t *p)
{
  return l4shmc_wait_chunk_to(p, L4_IPC_NEVER);
}

L4_CV L4_INLINE long
l4shmc_attach_signal(l4shmc_area_t *shmarea,
                     const char *signal_name,
                     l4_cap_idx_t thread,
                     l4shmc_signal_t *signal)
{
  return l4shmc_attach_signal_to(shmarea, signal_name, thread, 0, signal);
}

L4_CV L4_INLINE long
l4shmc_get_signal(l4shmc_area_t *shmarea,
                  const char *signal_name,
                  l4shmc_signal_t *signal)
{
  return l4shmc_get_signal_to(shmarea, signal_name, 0, signal);
}

L4_CV L4_INLINE long
l4shmc_get_chunk(l4shmc_area_t *shmarea,
                 const char *chunk_name,
                 l4shmc_chunk_t *chunk)
{
  return l4shmc_get_chunk_to(shmarea, chunk_name, 0, chunk);
}


L4_CV L4_INLINE long
l4shmc_chunk_ready(l4shmc_chunk_t *chunk, l4_umword_t size)
{
  chunk->_chunk->_size = size;
  __sync_synchronize();
  chunk->_chunk->_status = L4SHMC_CHUNK_READY;
  return L4_EOK;
}

L4_CV L4_INLINE long
l4shmc_trigger(l4shmc_signal_t *s)
{
  return l4_ipc_error(l4_irq_trigger(s->_sigcap), l4_utcb());
}

L4_CV L4_INLINE long
l4shmc_chunk_ready_sig(l4shmc_chunk_t *chunk, l4_umword_t size)
{
  l4shmc_chunk_ready(chunk, size);
  return l4shmc_trigger(chunk->_sig);
}

L4_CV L4_INLINE void *
l4shmc_chunk_ptr(l4shmc_chunk_t *p)
{
  return (void *)(((l4shmc_chunk_desc_t *)(p->_chunk->_offset +
                   (l4_addr_t)p->_shm->_local_addr))->payload);
}

L4_CV L4_INLINE l4shmc_signal_t *
l4shmc_chunk_signal(l4shmc_chunk_t *chunk)
{
  return chunk->_sig;
}

L4_CV L4_INLINE l4_cap_idx_t
l4shmc_signal_cap(l4shmc_signal_t *signal)
{
  return signal->_sigcap;
}

L4_CV L4_INLINE long
l4shmc_chunk_size(l4shmc_chunk_t *p)
{
  l4_umword_t s = p->_chunk->_size;
  if (s > p->_capacity)
    return -L4_EIO;
  return s;
}

L4_CV L4_INLINE long
l4shmc_chunk_capacity(l4shmc_chunk_t *p)
{
  return p->_capacity;
}

L4_CV L4_INLINE long
l4shmc_chunk_try_to_take(l4shmc_chunk_t *chunk)
{
  if (!l4util_cmpxchg(&chunk->_chunk->_status,
                      L4SHMC_CHUNK_CLEAR, L4SHMC_CHUNK_BUSY))
    return -L4_EPERM;
  return L4_EOK;
}

L4_CV L4_INLINE long
l4shmc_chunk_consumed(l4shmc_chunk_t *chunk)
{
  chunk->_chunk->_status = L4SHMC_CHUNK_CLEAR;
  return L4_EOK;
}


L4_CV L4_INLINE long
l4shmc_is_chunk_ready(l4shmc_chunk_t *chunk)
{
  return chunk->_chunk->_status == L4SHMC_CHUNK_READY;
}

L4_CV L4_INLINE long
l4shmc_is_chunk_clear(l4shmc_chunk_t *chunk)
{
  return chunk->_chunk->_status == L4SHMC_CHUNK_CLEAR;
}

L4_CV L4_INLINE long
l4shmc_check_magic(l4shmc_chunk_t *chunk)
{
  return chunk->_chunk->_magic == L4SHMC_CHUNK_MAGIC;
}

L4_CV L4_INLINE long
l4shmc_area_size(l4shmc_area_t *shmarea)
{
  return l4re_ds_size(shmarea->_shm_ds);
}
