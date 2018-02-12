/*****************************************************************************/
/**
 * \file    slab/include/slab.h
 * \brief   Slab allocator API.
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

/* This version supports multi-page slabs (slab size stored in cache.slab_size)
 * and tries to follow Bonwicks original suggestion to limit internal
 * fragmentation to 1/8 (12.5 %).
 *                                    -- Krishna
 */

#ifndef _L4SLAB_SLAB_H
#define _L4SLAB_SLAB_H

/* L4env includes */
#include <l4/sys/linkage.h>
#include <l4/sys/l4int.h>
#include <l4/sys/compiler.h>

/*****************************************************************************
 *** typedefs
 *****************************************************************************/

/**
 * Slab descriptor type
 */
typedef struct l4slab_slab l4slab_slab_t;

/**
 * \brief   Slab cache descriptor type
 * \ingroup api_init
 */
typedef struct l4slab_cache l4slab_cache_t;

/**
 * \brief   Cache grow callback function
 * \ingroup api_init
 *
 * \param   cache        Descriptor of the slab cache which requests
 *                       the memory
 * \retval  data         Slab user data pointer, the contents is returned
 *                       with the slab to the release callback function.
 *
 * \return  Pointer to the allocated slab.
 *
 * This function is called by a slab cache to allocate a new slab for the
 * cache. It must return a pointer to a memory area with the size of
 * cache->slab_size and which is slab_size aligned.
 */
typedef L4_CV void * (* l4slab_grow_fn_t) (l4slab_cache_t * cache,
                                           void ** data);

/**
 * \brief   Cache release callback function
 * \ingroup api_init
 *
 * \param   cache        Slab cache descriptor
 * \param   buffer       Slab address
 * \param   data         Slab user data pointer
 *
 * Cache release callback function. It is called by a slab cache to release
 * slabs which are no longer needed by the cache. Slab have a size of
 * cache->slab_size!
 */
typedef L4_CV void (* l4slab_release_fn_t) (l4slab_cache_t * cache,
                                            void * buffer, void * data);

/**
 * Slab cache descriptor
 */
struct l4slab_cache
{
  l4_size_t            obj_size;   ///< size of cache objects
  l4_size_t            slab_size;  ///< size of one slab
  int                  num_objs;   ///< number of objects per slab
  int                  num_slabs;  ///< number of slabs in cache
  int                  num_free;   ///< number of unused slabs
  int                  max_free;   ///< max. allowed unused slabs

  l4slab_slab_t *      slabs_full; ///< list of completely used slabs
  l4slab_slab_t *      slabs_part; ///< list of partially used slabs
  l4slab_slab_t *      slabs_free; ///< list of unused slabs

  l4slab_grow_fn_t     grow_fn;    ///< cache grow callback
  l4slab_release_fn_t  release_fn; ///< slab release callback

  void *               data;       ///< application data pointer
};

#define L4SLAB_LOG_CACHE(cp) do { \
	LOG("\033[34;1mcache at %p\033[0m", cp); \
	LOG("\033[34;1m  obj_size %u\033[0m", cp->obj_size); \
	LOG("\033[34;1m  slab_size %u\033[0m", cp->slab_size); \
	LOG("\033[34;1m  objs per slab %d\033[0m", cp->num_objs); \
	LOG("\033[34;1m  num_free %d\033[0m", cp->num_free); \
	LOG("\033[34;1m  data ptr %p\033[0m", cp->data); \
} while (0)


/*****************************************************************************
 *** prototypes
 *****************************************************************************/

__BEGIN_DECLS;

/*****************************************************************************/
/**
 * \brief   Initialize slab cache.
 * \ingroup api_init
 *
 * \param   cache        Slab cache descriptor
 * \param   size         Size of the cache objects
 * \param   max_free     Maximum number of free slabs allowed in the cache.
 *                       If more slabs in the slab cache are freed, they are
 *                       released  (if a release callback function is
 *                       specified).
 * \param   grow_fn      Cache grow callback function, called by the slab cache
 *                       to allocate new buffers for the cache. If no function
 *                       is  specified the cache cannot allocate buffers on
 *                       demand.
 * \param   release_fn   Slab release callback function, called by the cache to
 *                       release unused buffers. If no function is specified
 *                       unused buffers are not released.
 *
 * \return  0 on success (initialized cache descriptor), error code otherwise:
 *          - -L4_EINVAL  size too big / invalid cache descriptor
 *
 * Setup cache descriptor. The function initializes the internal cache
 * descriptor structures, but does not allocate any memory. Memory can be
 * added using the l4slab_add_slab() function or memory is allocated
 * on demand by the cache if the grow callback function is specified.
 */
/*****************************************************************************/
L4_CV int
l4slab_cache_init(l4slab_cache_t * cache, l4_size_t size,
                  unsigned int max_free, l4slab_grow_fn_t grow_fn,
                  l4slab_release_fn_t release_fn);

/*****************************************************************************/
/**
 * \brief   Destroy slab cache
 * \ingroup api_init
 *
 * \param   cache        Cache descriptor
 *
 * Release slab descriptor and free all allocated memory. This function is
 * only useful if a release callback function is specified for the cache,
 * otherwise it has no effect.
 */
/*****************************************************************************/
L4_CV void
l4slab_destroy(l4slab_cache_t * cache);

/*****************************************************************************/
/**
 * \brief   Allocate object
 * \ingroup api_alloc
 *
 * \param   cache        Cache descriptor
 *
 * \return pointer to object, NULL if allocation failed.
 */
/*****************************************************************************/
L4_CV void *
l4slab_alloc(l4slab_cache_t * cache);

/*****************************************************************************/
/**
 * \brief   Release object
 * \ingroup api_alloc
 *
 * \param   cache        Cache descriptor
 * \param   objp         Pointer to object
 */
/*****************************************************************************/
L4_CV void
l4slab_free(l4slab_cache_t * cache, void * objp);

/*****************************************************************************/
/**
 * \brief   Add a slab to the slab cache
 * \ingroup api_init
 *
 * \param   cache        Cache descriptor
 * \param   buffer       Pointer to new slab
 * \param   data         Application data
 *
 * Add the slab (buffer) to the slab cache. Buffer must be
 * cache->slab_size-sized and slab_size aligned in memory.
 */
/*****************************************************************************/
L4_CV void
l4slab_add_slab(l4slab_cache_t * cache, void * buffer, void * data);

/*****************************************************************************/
/**
 * \brief   Set cache application data pointer.
 * \ingroup api_init
 *
 * \param   cache        Cache descriptor
 * \param   data         Application data pointer
 */
/*****************************************************************************/
L4_CV void
l4slab_set_data(l4slab_cache_t * cache, void * data);

/*****************************************************************************/
/**
 * \brief   Get cache application data.
 * \ingroup api_init
 *
 * \param   cache        Cache descriptor
 *
 * \return Application data pointer, NULL if invalid cache descriptor or no
 *         data pointer set.
 */
/*****************************************************************************/
L4_CV void *
l4slab_get_data(l4slab_cache_t * cache);

/*****************************************************************************/
/**
 * \brief   Dump cache slab list
 * \ingroup api_debug
 *
 * \param   cache        Cache descriptor
 * \param   dump_free    Dump free list of slabs
 */
/*****************************************************************************/
L4_CV void
l4slab_dump_cache(l4slab_cache_t * cache, int dump_free);

/*****************************************************************************/
/**
 * \brief   Dump cache free slab list
 * \ingroup api_debug
 *
 * \param   cache        Cache descriptor
 */
/*****************************************************************************/
L4_CV void
l4slab_dump_cache_free(l4slab_cache_t * cache);

__END_DECLS;

#endif /* !_L4SLAB_SLAB_H */
