/*****************************************************************************/
/**
 * \file   slab/lib/src/slab.c
 * \brief  Simple slab memory allocator
 *
 * \date   2006-12-18
 * \author Lars Reuther <reuther@os.inf.tu-dresden.de>
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *
 * Simple Slab-based memory allocator.
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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* L4env includes */
#include <l4/sys/l4int.h>
#include <l4/sys/consts.h>
#include <l4/sys/err.h>
#include <l4/util/l4_macros.h>
#include <l4/util/bitops.h>

/* private includes */
#include <l4/slab/slab.h>
#include "__slab.h"
#include "__debug.h"

/*****************************************************************************
 *** Helpers
 *****************************************************************************/

/*****************************************************************************/
/**
 * \brief  Add slab to cache.
 *
 * \param  cache         Cache descriptor
 * \param  buffer        New slab backing store
 * \param  data          Application data
 */
/*****************************************************************************/
static void
__add_slab(l4slab_cache_t * cache, void * buffer, void * data)
{
  l4_addr_t * p;
  l4slab_slab_t * slab;
  int i;

  /* setup new slab */
  slab = slab_from_pointer(cache, buffer);
  slab->num_free = cache->num_objs;
  slab->cache = cache;
  slab->data = data;

  /* create object free list */
  slab->free_objs = buffer;
  p = buffer;
  for (i = 0; i < (slab->num_free - 1); i++)
    {
      *p = (l4_addr_t)(p + (cache->obj_size / sizeof(l4_addr_t)));
      p = (l4_addr_t *)*p;
    }
  *p = 0;

  /* insert slab in unused slabs list */
  slab->prev = NULL;
  slab->next = cache->slabs_free;
  if (slab->next != NULL)
    slab->next->prev = slab;
  cache->slabs_free = slab;
  cache->num_slabs++;
  cache->num_free++;
}

/*****************************************************************************/
/**
 * \brief Grow cache
 *
 * \param  cache         Cache descriptor
 *
 * \return 0 on success, -1 if allocation failed.
 */
/*****************************************************************************/
static int
__grow_cache(l4slab_cache_t * cache)
{
  void * buffer;
  void * data = NULL;

  /* grow function? */
  if (cache->grow_fn == NULL)
    return -1;

  /* allocate new slab */
  buffer = cache->grow_fn(cache, &data);
  if (buffer == NULL)
    return -1;
  assert(((l4_addr_t)buffer & (cache->slab_size - 1)) == 0);

  if (DEBUG_SLAB_GROW)
    printf("new slab at 0x"l4_addr_fmt"\n", (l4_addr_t)buffer);

  /* add slab to slab cache */
  __add_slab(cache, buffer, data);

  /* done */
  return 0;
}

/*****************************************************************************/
/**
 * \brief Shrink cache
 *
 * \param  cache         Cache descriptor
 */
/*****************************************************************************/
static void
__shrink_cache(l4slab_cache_t * cache)
{
  l4slab_slab_t * slab;
  void * buffer, * data;

  if (cache->release_fn == NULL)
    /* cannot shrink, no release callback */
    return;

  /* shrink */
  slab = cache->slabs_free;
  while (cache->num_free > cache->max_free)
    {
      if (slab == NULL)
        {
          /* something's wrong: free counter > 0, but no free slab found */
          printf("L4slab: corrupted slab free list!\n");
          exit(1);
          return;
        }

      assert(slab->num_free == cache->num_objs
             && "slab from free list is really free");
      assert(slab->prev == NULL
             && "first slab in free list has no predecessor");

      /* remove from free list */
      cache->slabs_free = slab->next;
      if (cache->slabs_free != NULL)
        cache->slabs_free->prev = NULL;

      /* update stats */
      cache->num_slabs--;
      cache->num_free--;

      /* release slab */
      data = slab->data;
      buffer = buffer_from_pointer(cache, slab);
      cache->release_fn(cache, buffer, data);

      slab = cache->slabs_free;
    }
}

/*****************************************************************************/
/**
 * \brief Relink slab
 *
 * \param  slab    Pointer to slab
 * \param  olist   Pointer to old list head
 * \param  nlist   Pointer to new list head
 */
/*****************************************************************************/
static void
relink(struct l4slab_slab *slab,
       struct l4slab_slab **olist, struct l4slab_slab **nlist)
{
  assert(slab);
  assert(olist);
  assert(nlist);

  /* remove from old list */
  if (slab->prev)
    slab->prev->next = slab->next;
  if (slab->next)
    slab->next->prev = slab->prev;
  if (slab == *olist)
    *olist = slab->next;

  slab->prev = NULL;

  /* add to new list */
  slab->next = *nlist;
  if (slab->next)
    slab->next->prev = slab;
  *nlist = slab;
}

/*****************************************************************************
 *** API functions
 *****************************************************************************/

/*****************************************************************************/
/**
 * \brief  Initialize slab cache.
 *
 * \param  cache         Cache descriptor
 * \param  obj_size      Size of cache objects
 * \param  max_free      Max. allowed free slabs, if more free slabs get
 *                       completely free, they are released (if a release
 *                       callback function is specified)
 * \param  grow_fn       Cache grow callback function
 * \param  release_fn    Slab release callback function
 *
 * \return 0 on success (initialized cache descriptor), error code otherwise:
 *         - -L4_EINVAL  size too big / invalid cache descriptor
 *
 * Setup cache descriptor. The function initializes the internal cache
 * descriptor structures, but does not allocate any memory. Memory can be
 * added using the l4slab_add_slab() function or memory is allocated
 * on demand by a cache if the grow callback function is specified.
 * Slab dimension is calculated following Bonwick's proposal for a maximum of
 * 12.5 % internal fragmentation - so 8 objects per slab is the "emperical
 * sweet-spot in the trade-off between internal and external fragmentation".
 */
/*****************************************************************************/
L4_CV int
l4slab_cache_init(l4slab_cache_t * cache, l4_size_t obj_size,
                  unsigned int max_free, l4slab_grow_fn_t grow_fn,
                  l4slab_release_fn_t release_fn)
{
  /* sanity checks */
  if (cache == NULL || obj_size == 0)
    return -L4_EINVAL;

  /* align size to pointer size */
  obj_size = (obj_size + (sizeof(l4_addr_t) - 1)) & ~(sizeof(l4_addr_t) - 1);

  /* calculate slab size */
  l4_size_t slab_size = obj_size * 8 + sizeof(struct l4slab_slab);
            slab_size = (slab_size + L4_PAGESIZE - 1) & L4_PAGEMASK;

  /* check of 2^n slab_size, required for getting size aligned slabs*/
  l4_size_t most = l4util_bsr(slab_size);
  if (most != (l4_size_t)l4util_bsf(slab_size)) {
    slab_size = 1 << (most + 1);
  }
  
  /* setup descriptor */
  cache->obj_size = obj_size;
  cache->slab_size = slab_size;
  cache->num_objs = (slab_size - sizeof(struct l4slab_slab)) / obj_size;
  cache->num_slabs = 0;
  cache->num_free = 0;
  cache->max_free = max_free;

  cache->slabs_full = cache->slabs_part = cache->slabs_free = NULL;

  cache->grow_fn = grow_fn;
  cache->release_fn = release_fn;

  cache->data = NULL;

  if (DEBUG_SLAB_INIT)
    printf("object size = %zu, slab size = %zu\n"
                        "objects per slab = %d, sizeof(struct l4slab_slab) = %zd\n",
                        obj_size, slab_size, cache->num_objs, sizeof(struct l4slab_slab));

  /* done */
  return 0;
}

/*****************************************************************************/
/**
 * \brief  Destroy slab cache
 *
 * \param  cache         Cache descriptor
 *
 * Release slab descriptor and free all allocated memory. This function is
 * only useful if a release callback function is specified for the cache,
 * otherwise it has no effect.
 */
/*****************************************************************************/
L4_CV void
l4slab_destroy(l4slab_cache_t * cache)
{
  l4slab_slab_t * slabs[3] = { cache->slabs_full, cache->slabs_part, cache->slabs_free };
  l4slab_slab_t * slab;
  void * ptr, * data;

  if ((cache == NULL) || (cache->release_fn == NULL))
    return;

  /* empty all lists */
  unsigned i;
  for (i = 0; i < sizeof(slabs)/sizeof(*slabs); ++i)
    {
      slab = slabs[i];
      while (slab)
        {
          data = slab->data;
          ptr = buffer_from_pointer(cache, slab);
          slab = slab->next;

          /* release slab */
          cache->release_fn(cache, ptr, data);
        }
    }
}

/*****************************************************************************/
/**
 * \brief  Allocate object
 *
 * \param  cache         Cache descriptor
 *
 * \return pointer to object, NULL if allocation failed.
 */
/*****************************************************************************/
L4_CV void *
l4slab_alloc(l4slab_cache_t * cache)
{
  int ret;
  l4slab_slab_t * slab;
  l4slab_slab_t **olist;         /* old list slab was linked to */
  l4slab_slab_t **nlist = NULL;  /* new list slab is linked to */
  void * objp;

  if (cache == NULL)
    return NULL;

  /* get slab which contains free objects */
  if (cache->slabs_part != NULL)
    {
      /* common case: partially used slab */
      slab = cache->slabs_part;
      olist = &cache->slabs_part;
    }
  else if (cache->slabs_free != NULL)
    {
      /* use unused slab */
      slab = cache->slabs_free;
      olist = &cache->slabs_free;
    }
  else
    {
      /* need to allocate new slab */
      ret = __grow_cache(cache);
      if (ret < 0)
        return NULL;
      slab = cache->slabs_free;
      olist = &cache->slabs_free;
    }

  /* allocate object, just use the first slab from the caches free list */
  objp = slab->free_objs;
  slab->free_objs = (void *)*((l4_addr_t *)objp);
  slab->num_free--;

  /* catch special cases */
  if (slab->num_free == cache->num_objs - 1)
    {
      /* used first object of a free slab */
      cache->num_free--;
      nlist = &cache->slabs_part;
    }
  else if (slab->num_free == 0)
    {
      /* slab completely used */
      nlist = &cache->slabs_full;
    }

  /* handle special cases */
  if (nlist)
    relink(slab, olist, nlist);

  /* done */
  return objp;
}

/*****************************************************************************/
/**
 * \brief  Release object
 *
 * \param  cache         Cache descriptor
 * \param  objp          Pointer to object
 */
/*****************************************************************************/
L4_CV void
l4slab_free(l4slab_cache_t * cache, void * objp)
{
  l4slab_slab_t * slab;
  l4slab_slab_t **olist;         /* old list slab was linked to */
  l4slab_slab_t **nlist = NULL;  /* new list slab is linked to */

  /* sanity */
  if ((cache == NULL) || (objp == NULL))
    return;

  /* get slab descriptor */
  slab = slab_from_pointer(cache, objp);

  /* correct cache? */
  assert(cache == slab->cache);

  /* insert obj in free list */
  *((l4_addr_t *)objp) = (l4_addr_t)slab->free_objs;
  slab->free_objs = objp;
  slab->num_free++;

 
  /* catch special cases */
  if (slab->num_free == 1)
    {
      /* first free object in slab */
      olist = &cache->slabs_full;
      nlist = &cache->slabs_part;
    }
  else if (slab->num_free == cache->num_objs)
    {
      /* slab completely freed */
      cache->num_free++;
      olist = &cache->slabs_part;
      nlist = &cache->slabs_free;
    }

  /* handle special cases */
  if (nlist)
    relink(slab, olist, nlist);

  /* limit cache size */
  if (cache->num_free > cache->max_free)
    __shrink_cache(cache);
}

/*****************************************************************************/
/**
 * \brief Add slab to slab cache
 *
 * \param  cache         Cache descriptor
 * \param  buffer        Pointer to new slab
 * \param  data          Application data
 *
 * Add the slab to the slab cache.
 */
/*****************************************************************************/
L4_CV void
l4slab_add_slab(l4slab_cache_t * cache, void * buffer, void * data)
{
  /* sanity checks */
  if ((cache == NULL) || (buffer == NULL))
    return;

  /* check alignment */
  if ((l4_addr_t)buffer & (cache->slab_size - 1))
    {
      fprintf(stderr, "slab buffer not size-aligned!\n");
      return;
    }

  /* add page */
  __add_slab(cache,buffer,data);
}

/*****************************************************************************/
/**
 * \brief  Set cache application data pointer.
 *
 * \param  cache         Cache descriptor
 * \param  data          Application data pointer
 */
/*****************************************************************************/
L4_CV void
l4slab_set_data(l4slab_cache_t * cache, void * data)
{
  if (cache == NULL)
    return;

  cache->data = data;
}

/*****************************************************************************/
/**
 * \brief Get cache application data.
 *
 * \param  cache         Cache descriptor
 *
 * \return Application data pointer, NULL if invalid cache descriptor or no
 *         data pointer set.
 */
/*****************************************************************************/
L4_CV void *
l4slab_get_data(l4slab_cache_t * cache)
{
  if (cache == NULL)
    return NULL;

  return cache->data;
}

/*****************************************************************************/
/**
 * \brief DEBUG: Dump cache slab list
 *
 * \param  cache         Cache descriptor
 * \param  dump_free     Dump free list of slabs
 */
/*****************************************************************************/
L4_CV void
l4slab_dump_cache(l4slab_cache_t * cache, int dump_free)
{
  l4slab_slab_t * slabs[3] = { cache->slabs_full, cache->slabs_part, cache->slabs_free };
  l4slab_slab_t * slab;
  l4_addr_t * p;

  if (cache == NULL)
    return;

  printf("  L4slab cache list, cache at 0x"l4_addr_fmt"\n",
            (l4_addr_t)cache);
  printf("  object size %lu, %u per slab, %d slab(s), %d free\n",
             (l4_addr_t)cache->obj_size,cache->num_objs,cache->num_slabs,
             cache->num_free);

  /* dump all lists */
  unsigned i;
  for (i = 0; i < sizeof(slabs)/sizeof(*slabs); ++i)
    {
      slab = slabs[i];
      while (slab != NULL)
        {
          printf("    slab at 0x"l4_addr_fmt", %d free object(s):\n",
                     (l4_addr_t)slab,slab->num_free);

          if (dump_free)
            {
              p = slab->free_objs;
              while (p != NULL)
                {
                  printf("      free at 0x"l4_addr_fmt"\n",(l4_addr_t)p);
                  p = (l4_addr_t *)*p;
                }
            }

          slab = slab->next;
        }
    }
}

/*****************************************************************************/
/**
 * \brief  DEBUG: Dump cache free slab list
 *
 * \param  cache         Cache descriptor
 */
/*****************************************************************************/
L4_CV void
l4slab_dump_cache_free(l4slab_cache_t * cache)
{
  l4slab_slab_t * slab;
  if (cache == NULL)
    return;

  printf("  L4slab cache free list, cache at 0x"l4_addr_fmt"\n",
             (l4_addr_t)cache);
  printf("  object size %lu, %u per slab, %d slab(s), %d free\n",
             (l4_addr_t)cache->obj_size,cache->num_objs,cache->num_slabs,
             cache->num_free);

  slab = cache->slabs_free;
  while (slab != NULL)
    {
      printf("    slab at 0x"l4_addr_fmt", %3d free object(s)\n",
                 (l4_addr_t)slab, slab->num_free);
      slab = slab->next;
    }
}
