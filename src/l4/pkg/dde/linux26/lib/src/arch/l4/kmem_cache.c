/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/*
 * \brief   Kmem_cache implementation
 *
 * In Linux 2.6 this resides in mm/slab.c.
 *
 * I'll disregard the following function currently...
 *
 * extern struct kmem_cache *kmem_find_general_cachep(size_t size, gfp_t gfpflags);
 * extern void *kmem_cache_zalloc(struct kmem_cache *, gfp_t);
 */

/* Linux */
#include <linux/slab.h>

/* DDEKit */
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/lock.h>


/*******************
 ** Configuration **
 *******************/

#define DEBUG_SLAB 0

#if DEBUG_SLAB
# define DEBUG_SLAB_ALLOC 1
#else
# define DEBUG_SLAB_ALLOC 0
#endif

/*
 * Kmem cache structure
 */
struct kmem_cache
{
	const char         *name;                 /**< cache name */
	unsigned            size;                 /**< obj size */

	struct ddekit_slab *ddekit_slab_cache;    /**< backing DDEKit cache */
	ddekit_lock_t      cache_lock;            /**< lock */
	void (*ctor)(void *);                     /**< object constructor */
};


/**
 * Return size of objects in cache
 */
unsigned int kmem_cache_size(struct kmem_cache *cache)
{
	return cache->size;
}


/**
 * Return name of cache
 */
const char *kmem_cache_name(struct kmem_cache *cache)
{
	return cache->name;
}


/**
 * kmem_cache_shrink - Shrink a cache.
 * @cachep: The cache to shrink.
 *
 * Releases as many slabs as possible for a cache.
 * To help debugging, a zero exit status indicates all slabs were released.
 */
int kmem_cache_shrink(struct kmem_cache *cache)
{
	/* noop */
	return 1;
}


/**
 * kmem_cache_free - Deallocate an object
 * @cachep: The cache the allocation was from.
 * @objp: The previously allocated object.
 *
 * Free an object which was previously allocated from this
 * cache.
 */
void kmem_cache_free(struct kmem_cache *cache, void *objp)
{
	ddekit_log(DEBUG_SLAB_ALLOC, "\"%s\" (%p)", cache->name, objp);

	ddekit_lock_lock(&cache->cache_lock);
	ddekit_slab_free(cache->ddekit_slab_cache, objp);
	ddekit_lock_unlock(&cache->cache_lock);
}


/**
 * kmem_cache_alloc - Allocate an object
 * @cachep: The cache to allocate from.
 * @flags: See kmalloc().
 *
 * Allocate an object from this cache.  The flags are only relevant
 * if the cache has no available objects.
 */
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	void *ret;

	ddekit_log(DEBUG_SLAB_ALLOC, "\"%s\" flags=%x", cache->name, flags);

	ddekit_lock_lock(&cache->cache_lock);
	ret = ddekit_slab_alloc(cache->ddekit_slab_cache);
	ddekit_lock_unlock(&cache->cache_lock);

	// XXX: is it valid to run ctor AND memset to zero?
	if (flags & __GFP_ZERO)
		memset(ret, 0, cache->size);
	else if (cache->ctor)
		cache->ctor(ret);

	return ret;
}


/**
 * kmem_cache_destroy - delete a cache
 * @cachep: the cache to destroy
 *
 * Remove a struct kmem_cache object from the slab cache.
 * Returns 0 on success.
 *
 * It is expected this function will be called by a module when it is
 * unloaded.  This will remove the cache completely, and avoid a duplicate
 * cache being allocated each time a module is loaded and unloaded, if the
 * module doesn't have persistent in-kernel storage across loads and unloads.
 *
 * The cache must be empty before calling this function.
 *
 * The caller must guarantee that noone will allocate memory from the cache
 * during the kmem_cache_destroy().
 */
void kmem_cache_destroy(struct kmem_cache *cache)
{
	ddekit_log(DEBUG_SLAB, "\"%s\"", cache->name);

	ddekit_slab_destroy(cache->ddekit_slab_cache);
	ddekit_simple_free(cache);
}


/**
 * kmem_cache_create - Create a cache.
 * @name: A string which is used in /proc/slabinfo to identify this cache.
 * @size: The size of objects to be created in this cache.
 * @align: The required alignment for the objects.
 * @flags: SLAB flags
 * @ctor: A constructor for the objects.
 *
 * Returns a ptr to the cache on success, NULL on failure.
 * Cannot be called within a int, but can be interrupted.
 * The @ctor is run when new pages are allocated by the cache
 * and the @dtor is run before the pages are handed back.
 *
 * @name must be valid until the cache is destroyed. This implies that
 * the module calling this has to destroy the cache before getting unloaded.
 *
 * The flags are
 *
 * %SLAB_POISON - Poison the slab with a known test pattern (a5a5a5a5)
 * to catch references to uninitialised memory.
 *
 * %SLAB_RED_ZONE - Insert `Red' zones around the allocated memory to check
 * for buffer overruns.
 *
 * %SLAB_HWCACHE_ALIGN - Align the objects in this cache to a hardware
 * cacheline.  This can be beneficial if you're counting cycles as closely
 * as davem.
 */
struct kmem_cache * kmem_cache_create(const char *name, size_t size, size_t align,
                                      unsigned long flags,
                                      void (*ctor)(void *))
{
	ddekit_log(DEBUG_SLAB, "\"%s\" obj_size=%d", name, size);

	struct kmem_cache *cache;

	if (!name) {
		printk("kmem_cache name reqeuired\n");
		return 0;
	}

	cache = ddekit_simple_malloc(sizeof(*cache));
	if (!cache) {
		printk("No memory for slab cache\n");
		return 0;
	}

	/* Initialize a physically contiguous cache for kmem */
	if (!(cache->ddekit_slab_cache = ddekit_slab_init(size, 1))) {
		printk("DDEKit slab init failed\n");
		ddekit_simple_free(cache);
		return 0;
	}

	cache->name = name;
	cache->size = size;
	cache->ctor = ctor;

	ddekit_lock_init_unlocked(&cache->cache_lock);

	return cache;
}
