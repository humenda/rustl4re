/*
 * \brief   Memory subsystem
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel <doebel@tudos.org>
 * \date    2006-11-03
 *
 * The memory subsystem provides the backing store for DMA-able memory via
 * large malloc and slabs.
 *
 * FIXME check thread-safety and add locks where appropriate
 */

#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/printf.h>

#include <cstdlib>

#include <l4/sys/compiler.h>
#include <l4/crtn/initpriorities.h>
#include <l4/slab/slab.h>
#include <l4/util/atomic.h>
#include <l4/util/bitops.h>
#include <l4/util/util.h>
#include <l4/util/macros.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/mem_alloc>
#include <l4/re/rm>

#include "internal.h"

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
	bool contig;
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

namespace DDEKit
{
static void *slab_allocate_new_region(long const size, long const mem_flags,
                                      long const attach_flags)
{
	void * ret = NULL;
	long   err = 0;

    L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!ds.is_valid())
		goto out;

	err =  L4Re::Env::env()->mem_alloc()->alloc(size, ds, mem_flags);
	if (err < 0)
		goto out;

	err = L4Re::Env::env()->rm()->attach(&ret, size, attach_flags,
                                             L4::Ipc::make_cap(ds, (attach_flags & L4Re::Rm::Read_only)
                                                                   ? L4_CAP_FPAGE_RO
                                                                   : L4_CAP_FPAGE_RW),
                                             0, l4util_log2(size) + 1);
	if (!ret) enter_kdebug("slab_allocate_new_region failed");

out:
	return ret;
}


static void slab_release_region(void *addr)
{
	L4::Cap<L4Re::Dataspace> ds;
	int err = L4Re::Env::env()->rm()->detach((l4_addr_t)addr, &ds);

	if (err < 0)
		DDEKit::panic("Detach failed!");

	if (ds.is_valid())
		ds->release();
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
void ddekit_slab_setup_page_cache(unsigned const pages)
{
	/* FIXME just allowing to grow at the moment */
	while (pcache_num_entries < pages) {
		struct ddekit_pcache *new_entry, *new_contig;

		/* create new list element */
		new_entry = (struct ddekit_pcache *)DDEKit::simple_malloc(sizeof(*new_entry));
		new_contig = (struct ddekit_pcache *)DDEKit::simple_malloc(sizeof(*new_contig));

		/* insert into lists of unused cache entries */
		do {
			new_entry->next = pcache_free;
		} while (!l4util_cmpxchg32((l4_uint32_t*)&pcache_free,
		                           (l4_uint32_t)new_entry->next,
		                           (l4_uint32_t)new_entry));

		do {
			new_contig->next = pcache_free_contig;
		} while (!l4util_cmpxchg32((l4_uint32_t*)&pcache_free_contig,
		                           (l4_uint32_t)new_entry->next,
		                           (l4_uint32_t)new_entry));

		/* increment number of list elements */
		l4util_inc32(&pcache_num_entries);
	}
}


/**
 * Try to allocate a new page from the pcache.
 */
static inline struct ddekit_pcache *_try_from_cache(bool const contig)
{
	struct ddekit_pcache *head = NULL;
	struct ddekit_pcache *the_cache = contig ? pcache_used_contig : pcache_used;

	do {
		head = the_cache;
		if (!head) break;
	} while (!l4util_cmpxchg32((l4_uint32_t*)&the_cache, (l4_uint32_t)head,
	                           (l4_uint32_t)head->next));

	return head;
}


static inline void _add_to_cache(struct ddekit_pcache *entry, bool const contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_used_contig : pcache_used;
	do {
		entry->next = the_cache;
	} while (! l4util_cmpxchg32((l4_uint32_t*)&the_cache, (l4_uint32_t)entry->next,
	                            (l4_uint32_t)entry));
}

/**
 * Return free entry to cached entry list.
 */
static inline void _free_cache_entry(struct ddekit_pcache *entry, bool const contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_free_contig : pcache_free;
	do {
		entry->next = the_cache;
	} while (!l4util_cmpxchg32((l4_uint32_t*)&the_cache, (l4_uint32_t)entry->next,
	                           (l4_uint32_t)entry));
}


static inline struct ddekit_pcache *_get_free_cache_entry(bool const contig)
{
	struct ddekit_pcache *the_cache = contig ? pcache_free_contig : pcache_free;
	struct ddekit_pcache *head = NULL;

	do {
		head = the_cache;
		if (!head) break;
	} while (!l4util_cmpxchg32((l4_uint32_t*)&the_cache, (l4_uint32_t)head,
	                           (l4_uint32_t) head->next));

	return head;
}


/*******************************
 ** Slab cache implementation **
 *******************************/

namespace DDEKit
{
	class Slab_impl : public DDEKit::Slab, public DDEKitObject
	{
	public:
		explicit Slab_impl(int const size, bool const contiguous);
		virtual ~Slab_impl();

		virtual void set_data(l4_addr_t data);
		virtual l4_addr_t get_data();
		virtual l4_addr_t alloc();
		virtual void free(l4_addr_t ptr);
		virtual bool contiguous() { return _contig; }

	private:
		explicit Slab_impl();
		Slab_impl(const Slab_impl&) { }
		Slab_impl& operator=(const Slab_impl&) { return *this; }

		l4slab_cache_t _cache;
		bool           _contig;
		l4_addr_t      _data;
	};
}


DDEKit::Slab* DDEKit::Slab::create(int const size, bool const contig)
{
	return new DDEKit::Slab_impl(size, contig);
}

namespace DDEKit
{
/** 
 * Get the ddekit slab for a given L4 slab.  This is stored in l4slab's
 * data pointer element.
 */
static inline DDEKit::Slab_impl* slab_from_l4slab(l4slab_cache_t *s)
{
	return reinterpret_cast<DDEKit::Slab_impl*>(l4slab_get_data(s));
}


/**
 * Grow slab cache
 */
extern "C" void *_slab_grow(l4slab_cache_t *cache, void **data)
{
	/* the page(s) to be returned */
	void *res = NULL;
	/* whether this cache needs physically contiguous pages */
	bool is_contig = DDEKit::slab_from_l4slab(cache)->contiguous();

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
		l4_addr_t dm_paddr;
		int num_pages = cache->slab_size / L4_PAGESIZE;
		long att_flags = L4Re::Rm::Eager_map | L4Re::Rm::Search_addr;
		long alloc_flags = L4Re::Mem_alloc::Pinned;

		if (is_contig)
			alloc_flags |= L4Re::Mem_alloc::Continuous;

		/* allocate and map new page(s) */
		res = DDEKit::slab_allocate_new_region(cache->slab_size, alloc_flags,
		                                       att_flags);
#if 0
		res = l4dm_mem_allocate_named(num_pages * L4_PAGESIZE,
		                              flags,
		                              "ddekit slab");
#endif
		if (res == NULL)
			DDEKit::panic("__grow: error allocating a new page\n");

#if 0
		/* physically contiguous pages need some special treatment */
		if (is_contig) {
			err = l4dm_mem_phys_addr(res, num_pages, &dm_paddr, 1, &tmp);
			if (err != 1)
				DDEKit::printf("__grow: error getting physical address of new page!");

			// XXX
			ddekit_pgtab_set_region(res, dm_paddr.addr, num_pages, PTE_TYPE_UMA);
		}
#endif
	}

	/* save pointer to cache in page for ddekit_slab_get_slab() */
	*data = cache;

	return res;
}

/**
 * Shrink slab cache
 */
extern "C" void _slab_shrink(l4slab_cache_t *cache, void *page, void *data)
{
	/* cache deallocated page here */
	struct ddekit_pcache *head = NULL;
	/* whether this cache needs physically contiguous pages */
	bool is_contig = DDEKit::slab_from_l4slab(cache)->contiguous();

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
#if 0
		if (is_contig)
			/* unset pte */
			ddekit_pgtab_clear_region(page, PTE_TYPE_UMA);
#endif

		/* free page */
		DDEKit::slab_release_region(page);
	}
}
}


DDEKit::Slab_impl::Slab_impl(int const size, bool const contiguous)
  : _contig(contiguous), _data(0)
{
	int err = l4slab_cache_init(&_cache, size, 0, DDEKit::_slab_grow, DDEKit::_slab_shrink);
	if (err) {
		DDEKit::printf("error initializing cache");
		DDEKit::panic("Slab_impl::Slab_impl");
	}

	l4slab_set_data(&_cache, this);
}


DDEKit::Slab_impl::~Slab_impl()
{
	l4slab_destroy(&_cache);
}


l4_addr_t DDEKit::Slab_impl::alloc()
{
	return reinterpret_cast<l4_addr_t>(l4slab_alloc(&_cache));
}

void DDEKit::Slab_impl::free(l4_addr_t ptr)
{
	return l4slab_free(&_cache, reinterpret_cast<void*>(ptr));
}

l4_addr_t DDEKit::Slab_impl::get_data()
{
	return _data;
}

void DDEKit::Slab_impl::set_data(l4_addr_t data)
{
	_data = data;
}


/**********************************
 ** Large block memory allocator **
 **********************************/

/**
 * Free large block of memory
 *
 * This is no useful for allocation < page size.
 */
extern "C" void ddekit_large_free(void *objp)
{
	/* clear PTEs */
#if 0
	ddekit_pgtab_clear_region(objp, PTE_TYPE_LARGE);
#endif

	/* release */
	DDEKit::slab_release_region(objp);
}


/**
 * Allocate large block of memory
 *
 * This is not useful for allocation < page size.
 */
extern "C" void *ddekit_large_malloc(int const size)
{
	void *res;
	int err;
	l4_size_t tmp;
	int pages;
	l4_addr_t dm_paddr;

	tmp  = l4_round_page(size);
	pages = tmp >> L4_PAGESHIFT;

	res = DDEKit::slab_allocate_new_region(tmp, L4Re::Mem_alloc::Continuous | L4Re::Mem_alloc::Pinned,
	                                       L4Re::Rm::Eager_map);

#if 0
	res = l4dm_mem_allocate_named(tmp,
	                              L4DM_CONTIGUOUS | L4DM_PINNED |
	                              L4RM_MAP | L4RM_LOG2_ALIGNED,
	                              "ddekit mem");
#endif
	if (! res)
		return NULL;

#if 0
	err = l4dm_mem_phys_addr(res, 1, &dm_paddr, 1, &tmp);
	if (err != 1)
		DDEKit::printf("ddekit_large_malloc: error getting physical address of new memory!\n");

			// XXX
	ddekit_pgtab_set_region(res, dm_paddr.addr, pages, PTE_TYPE_LARGE);
#endif

	return res;
}


/**
 * Allocate large block of memory (special interface)
 *
 * This is no useful for allocation < page size.
 *
 * FIXME implementation missing...
 */
extern "C" void *ddekit_contig_malloc(unsigned long const size,
                           unsigned long const low, unsigned long const high,
                           unsigned long const alignment, unsigned long const boundary)
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
			DDEKit::printf("contigmalloc: error getting physical address of new page!\n");
	
		/* set PTE */
		ddekit_set_ptes(res, dm_paddr.addr, pages, PTE_TYPE_CONTIG);
	}

	return res;
#else
	DDEKit::printf("%s: not implemented\n", __func__);
	return 0;
#endif
}

namespace DDEKit
{
static RegionManager *_rm = NULL;
void init_mm(void)
{
	_rm = DDEKit::RegionManager::create();
}

inline RegionManager *rm()
{
	return _rm;
}

}
