/*****************************************************************************/
/**
 * \file   slab/lib/include/__slab.h
 * \brief  Slab allocator internal types.
 *
 * \date   2006-12-18
 * \author Lars Reuther <reuther@os.inf.tu-dresden.de>
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 */
/*****************************************************************************/

/*
 * (c) 2006-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _SLAB___SLAB_H
#define _SLAB___SLAB_H

#include <l4/sys/l4int.h>

/* slab includes */
#include <l4/slab/slab.h>

/*****************************************************************************
 *** typedefs
 *****************************************************************************/

/**
 * Slab descriptor
 *
 * Slabs can be linked to one of the three slab cache lists: full, part, and
 * free.
 */
struct l4slab_slab
{
  int                    num_free;   ///< number of free objects in slab

  void *                 free_objs;  ///< free list

  l4slab_cache_t *       cache;      ///< pointer to slab cache descriptor

  struct l4slab_slab *   prev;       ///< previous slab in current list
  struct l4slab_slab *   next;       ///< next slab in current list

  void *                 data;       ///< client data
};


/**
 * Calculate pointer to slab struct from pointer in slab
 *
 * \param cache  slab cache this slab is linked to
 * \param p      pointer into slab
 *
 * The slab struct resides at the end of the slab and slab must be size aligned
 * in virtual memory.
 */
L4_INLINE struct l4slab_slab * slab_from_pointer(l4slab_cache_t *cache, void *p);
L4_INLINE struct l4slab_slab * slab_from_pointer(l4slab_cache_t *cache, void *p)
{
	l4_addr_t addr = (l4_addr_t) p;

	/* right after slab */
	addr = (addr + cache->slab_size) & ~(cache->slab_size - 1);

	/* slab struct resides at end of slab */
	addr -= sizeof(struct l4slab_slab);

	return (struct l4slab_slab *)addr;
}


/**
 * Calculate pointer to begin of slab buffer from pointer in slab
 *
 * \param cache  slab cache this slab is linked to
 * \param p      pointer into slab
 *
 * Slab must be size aligned in virtual memory.
 */
L4_INLINE void * buffer_from_pointer(l4slab_cache_t *cache, void *p);
L4_INLINE void * buffer_from_pointer(l4slab_cache_t *cache, void *p)
{
	l4_addr_t addr = (l4_addr_t) p;

	addr = addr & ~(cache->slab_size - 1);

	return (void *)addr;
}

#endif /* !_SLAB___SLAB_H */
