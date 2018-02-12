/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
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
#error Do not include l4/shm/types.h directly, use l4/shm/shm.h!
#endif

#include <l4/re/c/dataspace.h>

enum {
  L4SHMC_NAME_SIZE             = 15,
  L4SHMC_NAME_STRINGLEN        = L4SHMC_NAME_SIZE + 1,
  L4SHMC_CHUNK_NAME_SIZE       = 15,
  L4SHMC_CHUNK_NAME_STRINGLEN  = L4SHMC_CHUNK_NAME_SIZE + 1,
  L4SHMC_SIGNAL_NAME_SIZE      = 15,
  L4SHMC_SIGNAL_NAME_STRINGLEN = L4SHMC_SIGNAL_NAME_SIZE + 1,

  L4SHMC_CHUNK_READY = 2,
  L4SHMC_CHUNK_BUSY  = 1,
  L4SHMC_CHUNK_CLEAR = 0,

  L4SHMC_CHUNK_MAGIC = 0xfedcba98,
};

/* l4shmc_chunk_desc_t is shared among address spaces */
/* private: This data structure is hidden for the clients */
typedef struct {
  l4_umword_t _magic;        // magic
  l4_addr_t _offset;         // offset of chunk in shm-area
  l4_umword_t _capacity;     // capacity in bytes of chunk
  l4_umword_t _size;         // size of current payload
  l4_umword_t _status;       // status of chunk
  char _name[L4SHMC_CHUNK_NAME_STRINGLEN]; // name of chunk
  l4_addr_t _next;           // next chunk in shm-area, as absolute offset
  char payload[];
} l4shmc_chunk_desc_t;

/* l4shmc_area_t is local to one address space */
/* private: This data structure is hidden for the clients */
typedef struct {
  l4re_ds_t         _shm_ds;
  void             *_local_addr;
  char              _name[L4SHMC_NAME_STRINGLEN];
  l4_umword_t       _size;
} l4shmc_area_t;

/* l4shmc_signal_t is local to one address space */
typedef struct {
  l4_cap_idx_t        _sigcap;
} l4shmc_signal_t;

/* l4shmc_chunk_t is local to one address space */
typedef struct {
  l4shmc_chunk_desc_t  *_chunk;
  l4shmc_area_t        *_shm;
  l4shmc_signal_t      *_sig;
  l4_umword_t           _capacity;
} l4shmc_chunk_t;
