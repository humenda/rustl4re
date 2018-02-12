/*
 * \brief   Memory subsystem
 *
 * The memory subsystem provides the backing store for DMA-able memory via
 * large malloc and slabs.
 *
 * FIXME check thread-safety and add locks where appropriate
 */

/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/slab/slab.h>
#include <l4/util/atomic.h>
#include <l4/util/bitops.h>
#include <l4/util/util.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/mem_alloc>
#include <l4/re/rm>

#include <pthread-l4.h>

#define DEBUG 0

/****************
 ** Page cache **
 ****************/

/* FIXME revisit two-linked-lists approach - we could get rid of the
 * pcache_free lists, because these descriptors can also be allocated
 * whenever the need arises. (XXX: Check how this hurts performance.)
 */

/*****************************************************************************
  DDEKit maintains a list of free pages that can be used for growing
  existing DDEKit slabs. We distinguish between 2 types of pages: the ones
  that have been allocated from physically contiguous dataspaces and the the
  ones that have not.

  For each type of cache, we maintain two lists: a list of free pages that
  can be used to grow a cache (pcache_used) and a list of free pcache entries
  that can be used to store pages when they get freed (pcache_free). The
  reason for this is that by using these two lists, cache operations can be
  done atomically using cmpxchg.

  The function ddekit_slab_setup_page_cache() is used to tune the number of
  cached pages.
 ******************************************************************************/

/* page cache to minimize allocations from external servers */
struct ddekit_pcache
{
	struct ddekit_pcache *next;
	void *page;
	int contig;
};

/* head of single-linked list containing used page-cache entries, non-contiguous */
static struct ddekit_pcache *pcache_used;
/* head of single-linked list containing free page-cache entries, non-contiguous */
static struct ddekit_pcache *pcache_free;

/* head of single-linked list containing used page-cache entries, contiguous */
static struct ddekit_pcache *pcache_used_contig;
/* head of single-linked list containing free page-cache entries, contiguous */
static struct ddekit_pcache *pcache_free_contig;

/* maximum number of pages to cache. defaults to a minimum of 1 page
 * because having none hits the performance too hard. */
static l4_uint32_t pcache_num_entries = 1;

#include <l4/sys/kdebug.h>
static void *ddekit_slab_allocate_new_region(long size, long mem_flags,
                                             long attach_flags)
{
	void * ret = NULL;
	long   err = 0;

    L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
#if DEBUG
	ddekit_printf("\033[33m%s - cap %lx\033[0m\n", __func__, ds.cap());
#endif
	if (!ds.is_valid()) {
		goto out;
	}

	err =  L4Re::Env::env()->mem_alloc()->alloc(size, ds, mem_flags);
#if DEBUG
	ddekit_printf("\033[33mmem_alloc(size = %d, flags = %lx) = %d\033[0m\n", size, mem_flags, err);
#endif
	if (err < 0)
		goto out;

#if DEBUG
	ddekit_printf("\033[33mattach(ptr %p, size %d, flags %lx)\033[0m\n", ret, size, attach_flags | L4Re::Rm::Search_addr);
#endif
	err = L4Re::Env::env()->rm()->attach(&ret, size, attach_flags | L4Re::Rm::Search_addr,
                                             L4::Ipc::make_cap(ds, (attach_flags & L4Re::Rm::Read_only)
                                                                   ? L4_CAP_FPAGE_RO
                                                                   : L4_CAP_FPAGE_RW),
                                             0, l4util_log2(size) + 1);
#if DEBUG
	ddekit_printf("\033[33mattached ds to %p : %d\033[0m\n", ret, err);
#endif

	l4_addr_t phys;
	l4_size_t psize;
	err = ds->phys(0, phys, psize);
#if DEBUG
	ddekit_printf("\033[33mphys addr @ %p, err %d\033[0m\n", phys, err);
#endif
	if (!err) // got pinned memory
		ddekit_pgtab_set_region_with_size(ret, phys, psize, 0);
out:
	return ret;
}


static void ddekit_slab_release_region(void *addr)
{
	L4::Cap<L4Re::Dataspace> ds;
	int err = L4Re::Env::env()->rm()->detach((l4_addr_t)addr, &ds);
//	ddekit_printf("detach %d %lx\n", err, ds.cap());

	if (err < 0)
		ddekit_panic("Detach failed!");

	ddekit_pgtab_clear_region(addr, 0);

	if (ds.is_valid()) {
		ds->release();
		L4Re::Util::cap_alloc.free(ds);
	}
}

/**
 * Setup page cache for all slabs
 *
 * \param pages  maximal number of memory pages
 *
 * If the maximal number of unused pages is exceeded, subsequent deallocation
 * will be freed at the memory server. This page cache caches pages from all
 * slabs.
 */
EXTERN_C void ddekit_slab_setup_page_cache(unsigned pages)
{
	/* FIXME just allowing to grow at the moment */
	while (pcache_num_entries < pages) {
		struct ddekit_pcache *new_entry, *new_contig;

		/* create new list element */
		new_entry = (struct ddekit_pcache *) ddekit_simple_malloc(sizeof(*new_entry));
		new_contig = (struct ddekit_pcache *) ddekit_simple_malloc(sizeof(*new_contig));

		/* insert into lists of unused cache entries */
		do {
			new_entry->next = pcache_free;
		} while (!l4util_cmpxchg((l4_umword_t*)&pcache_free,
		                           (l4_umword_t)new_entry->next,
		                           (l4_umword_t)new_entry));

		do {
			new_contig->next = pcache_free_contig;
		} while (!l4util_cmpxchg((l4_umword_t*)&pcache_free_contig,
		                           (l4_umword_t)new_entry->next,
		                           (l4_umword_t)new_entry));

		/* increment number of list elements */
		l4util_inc32(&pcache_num_entries);
	}
}


/**
 * Try to allocate a new page from the pcache.
 */
static inline struct ddekit_pcache *_try_from_cache(int contig)
{
	struct ddekit_pcache *head = NULL;
	struct ddekit_pcache *the_cache = contig ? pcache_used_contig : pcache_used;

	do {
		head = the_cache;
		if (!head) break;
	} while (!l4util_cmpxchg((l4_umword_t*)&the_cache, (l4_umword_t)head,
	                           (l4_umword_t)head->next));

	return head;
}


static inline void _add_to_cache(struct ddekit_pcache *entry, int contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_used_contig : pcache_used;
	do {
		entry->next = the_cache;
	} while (! l4util_cmpxchg((l4_umword_t*)&the_cache, (l4_umword_t)entry->next,
	                            (l4_umword_t)entry));
}

/**
 * Return free entry to cached entry list.
 */
static inline void _free_cache_entry(struct ddekit_pcache *entry, int contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_free_contig : pcache_free;
	do {
		entry->next = the_cache;
	} while (!l4util_cmpxchg((l4_umword_t*)&the_cache, (l4_umword_t)entry->next,
	                           (l4_umword_t)entry));
}


static inline struct ddekit_pcache *_get_free_cache_entry(int contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_free_contig : pcache_free;
	struct ddekit_pcache *head = NULL;

	do {
		head = the_cache;
		if (!head) break;
	} while (!l4util_cmpxchg((l4_umword_t*)&the_cache, (l4_umword_t)head,
	                           (l4_umword_t) head->next));

	return head;
}


/*******************************
 ** Slab cache implementation **
 *******************************/

/* ddekit slab facilitates l4slabs */
struct ddekit_slab
{
	l4slab_cache_t  cache;
	pthread_mutex_t mtx;
	int             contiguous;
};


/** 
 * Get the ddekit slab for a given L4 slab. 
 *
 * We cheat here, because we know that the L4 slab is the first member
 * of the ddekit slab.
 */
static inline struct ddekit_slab *ddekit_slab_from_l4slab(l4slab_cache_t *s)
{
	return (struct ddekit_slab *)s;
}


/**
 * Grow slab cache
 */
EXTERN_C L4_CV void *_slab_grow(l4slab_cache_t *cache, void **data);
EXTERN_C L4_CV void *_slab_grow(l4slab_cache_t *cache, void **data)
{
	/* the page(s) to be returned */
	void *res = NULL;
	/* whether this cache needs physically contiguous pages */
	int is_contig = ddekit_slab_from_l4slab(cache)->contiguous;

	/* free cache entry, will be used afterwards */
	struct ddekit_pcache *head = NULL;

	/* We don't reuse pages for slabs > 1 page, because this makes caching
	 * somewhat harder. */
	if (cache->slab_size <= L4_PAGESIZE)
		/* try to aquire cached page */
		head = _try_from_cache(is_contig);

	if (head) {
		/* use cached page */
		res = head->page;
		/* this cache entry is now available */
		_free_cache_entry(head, is_contig);
	} else {
		/* allocate new page at memory server */
		long att_flags = L4Re::Rm::Eager_map;
		long alloc_flags = L4Re::Mem_alloc::Pinned;

		if (is_contig)
			alloc_flags |= L4Re::Mem_alloc::Continuous;

		/* allocate and map new page(s) */
		res = ddekit_slab_allocate_new_region(cache->slab_size, alloc_flags,
		                                      att_flags);
		if (res == NULL)
			ddekit_debug("__grow: error allocating a new page");
	}

	/* save pointer to cache in page for ddekit_slab_get_slab() */
	*data = cache;

	return res;
}

/**
 * Shrink slab cache
 */
EXTERN_C L4_CV
void _slab_shrink(l4slab_cache_t *cache, void *page, void *data);
EXTERN_C L4_CV
void _slab_shrink(l4slab_cache_t *cache, void *page, void *data)
{
        if (0)
	        ddekit_printf("%s: cache %p, page %p, data %p\n",
		              __func__, cache, page, data);
	/* cache deallocated page here */
	struct ddekit_pcache *head = NULL;
	/* whether this cache needs physically contiguous pages */
	int is_contig = ddekit_slab_from_l4slab(cache)->contiguous;

	/* we do not return slabs to the page cache, if they are larger than one page */
	if (cache->slab_size <= L4_PAGESIZE)
		/* try to aquire free cache entry */
		head = _get_free_cache_entry(is_contig);

	if (head) {
		/* use free cache entry to cache page */

		/* set the page info */
		head->page = page;

		/* this cache entry contains a free page */
		_add_to_cache(head, is_contig);
	} else {
		/* cache is full */
		
		// huh? clear_region() is called by slab_release_region() anyway...
#if 0
		if (is_contig)
			/* unset pte */
			ddekit_pgtab_clear_region(page, PTE_TYPE_UMA);
#endif

		/* free page */
		ddekit_slab_release_region(page);
	}
}


/**
 * Allocate object in slab
 */
EXTERN_C void *ddekit_slab_alloc(struct ddekit_slab * slab)
{
	void *ret;
	
	pthread_mutex_lock(&slab->mtx);
	ret = l4slab_alloc(&slab->cache);
	pthread_mutex_unlock(&slab->mtx);

	return ret;
}


/**
 * Free object in slab
 */
EXTERN_C void  ddekit_slab_free(struct ddekit_slab * slab, void *objp)
{
	pthread_mutex_lock(&slab->mtx);
	l4slab_free(&slab->cache, objp);
	pthread_mutex_unlock(&slab->mtx);
}


/**
 * Store user pointer in slab cache
 */
EXTERN_C void  ddekit_slab_set_data(struct ddekit_slab * slab, void *data)
{
	l4slab_set_data(&slab->cache, data);
}


/**
 * Read user pointer from slab cache
 */
EXTERN_C void *ddekit_slab_get_data(struct ddekit_slab * slab)
{
	return l4slab_get_data(&slab->cache);
}


/**
 * Destroy slab cache
 *
 * \param slab  pointer to slab cache structure
 */
EXTERN_C void  ddekit_slab_destroy (struct ddekit_slab * slab)
{
	l4slab_destroy(&slab->cache);
	ddekit_simple_free(slab);
}


/**
 * Initialize slab cache
 *
 * \param size          size of cache objects
 * \param contiguous    make this slab use physically contiguous memory
 *
 * \return pointer to new slab cache or 0 on error
 */
EXTERN_C struct ddekit_slab * ddekit_slab_init(unsigned size, int contiguous)
{
	struct ddekit_slab * slab;
	int err;

	slab = (struct ddekit_slab *) ddekit_simple_malloc(sizeof(*slab));
	pthread_mutex_init(&slab->mtx, NULL);

	err = l4slab_cache_init(&slab->cache, size, 0, _slab_grow, _slab_shrink);
	if (err) {
		ddekit_debug("error initializing cache");
		pthread_mutex_destroy(&slab->mtx);
		ddekit_simple_free(slab);
		return 0;
	}

	slab->contiguous = contiguous;
	
	return slab;
}


/**********************************
 ** Large block memory allocator **
 **********************************/

pthread_mutex_t large_alloc_mtx = PTHREAD_MUTEX_INITIALIZER;
/**
 * Free large block of memory
 *
 * This is no useful for allocation < page size.
 */
EXTERN_C void ddekit_large_free(void *objp)
{
	/* clear PTEs */
	//ddekit_pgtab_clear_region(objp, PTE_TYPE_LARGE);

	pthread_mutex_lock(&large_alloc_mtx);
	/* release */
	ddekit_slab_release_region(objp);
	pthread_mutex_unlock(&large_alloc_mtx);
}


/**
 * Allocate large block of memory
 *
 * This is not useful for allocation < page size.
 */
EXTERN_C void *ddekit_large_malloc(int size)
{
	void *res;

	pthread_mutex_lock(&large_alloc_mtx);
	size  = l4_round_page(size);

	res = ddekit_slab_allocate_new_region(size, L4Re::Mem_alloc::Continuous | L4Re::Mem_alloc::Pinned,
	                                      L4Re::Rm::Eager_map);

	pthread_mutex_unlock(&large_alloc_mtx);

	return res;
}


/**
 * Allocate large block of memory (special interface)
 *
 * This is no useful for allocation < page size.
 *
 * FIXME implementation missing...
 */
EXTERN_C void *ddekit_contig_malloc(unsigned long size __attribute__((unused)),
                           unsigned long low __attribute__((unused)),
                           unsigned long high __attribute__((unused)),
                           unsigned long alignment __attribute__((unused)),
                           unsigned long boundary __attribute__((unused)))
{
#if 0
	void *res;
	int err;
	int pages;
	l4_size_t tmp;
	l4dm_mem_addr_t dm_paddr;

	size  = l4_round_page(size);
	pages = size >> L4_PAGESHIFT;

	res = l4dm_mem_allocate_named(size, L4DM_CONTIGUOUS | L4DM_PINNED | L4RM_MAP | L4RM_LOG2_ALIGNED);

	if (res) {
		/* check if res meets low/high/alignment/boundary
		 * and panic if it is not the case
		 * XXXY
		 */

		/* get physical address */
		err = l4dm_mem_phys_addr(res, 1, &dm_paddr, 1, &tmp);
		if (err != 1)
			ddekit_debug("contigmalloc: error getting physical address of new page!\n");
	
		/* set PTE */
		ddekit_set_ptes(res, dm_paddr.addr, pages, PTE_TYPE_CONTIG);
	}

	return res;
#else
	ddekit_debug("%s: not implemented\n", __func__);
	return 0;
#endif
}
