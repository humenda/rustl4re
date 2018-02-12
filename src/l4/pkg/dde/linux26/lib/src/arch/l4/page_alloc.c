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
 * \brief   Page allocation
 *
 * In Linux 2.6 this resides in mm/page_alloc.c.
 *
 * This implementation is far from complete as it does not cover "struct page"
 * emulation. In Linux, there's an array of structures for all pages. In
 * particular, iteration works for this array like:
 *
 *   struct page *p = alloc_pages(3); // p refers to first page of allocation
 *   ++p;                             // p refers to second page
 *
 * There may be more things to cover and we should have a deep look into the
 * kernel parts we want to reuse. Candidates for problems may be file systems,
 * storage (USB, IDE), and video (bttv).
 */

/* Linux */
#include <linux/bootmem.h>
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/pagevec.h>
#include <linux/mm.h>
#include <asm/page.h>

/* DDEKit */
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/panic.h>

#include "local.h"

unsigned long max_low_pfn;
unsigned long min_low_pfn;
unsigned long max_pfn;

/*******************
 ** Configuration **
 *******************/

#define DEBUG_PAGE_ALLOC 0


/*
 * DDE page cache
 *
 * We need to store all pages somewhere (which in the Linux kernel is
 * performed by the huge VM infrastructure. Purpose for us is:
 *   - make virt_to_phys() work
 *   - enable external clients to hand in memory (e.g., a dm_phys
 *     dataspace and make it accessible as Linux pages to the DDE)
 */

#define DDE_PAGE_CACHE_SHIFT 	10
#define DDE_PAGE_CACHE_SIZE     (1 << DDE_PAGE_CACHE_SHIFT)
#define DDE_PAGE_CACHE_MASK     (DDE_PAGE_CACHE_SIZE - 1)

typedef struct
{
	struct hlist_node list;
	struct page *page;
} page_cache_entry;

static struct hlist_head dde_page_cache[DDE_PAGE_CACHE_SIZE];

/** Hash function to map virtual addresses to page cache buckets. */
#define VIRT_TO_PAGEHASH(a)           ((((unsigned long)a) >> PAGE_SHIFT) & DDE_PAGE_CACHE_MASK)


void dde_page_cache_add(struct page *p)
{
	unsigned int hashval = VIRT_TO_PAGEHASH(p->virtual);

	page_cache_entry *e = kmalloc(sizeof(page_cache_entry), GFP_KERNEL);

#if DEBUG_PAGE_ALLOC
	DEBUG_MSG("virt %p, hash: %x", p->virtual, hashval);
#endif

	e->page = p;
	INIT_HLIST_NODE(&e->list);

	hlist_add_head(&e->list, &dde_page_cache[hashval]);
}


void dde_page_cache_remove(struct page *p)
{
	unsigned int hashval = VIRT_TO_PAGEHASH(p->virtual);
	struct hlist_node *hn = NULL;
	struct hlist_head *h  = &dde_page_cache[hashval];
	page_cache_entry *e   = NULL;
	struct hlist_node *v  = NULL;

	hlist_for_each_entry(e, hn, h, list) {
		if ((unsigned long)e->page->virtual == ((unsigned long)p->virtual & PAGE_MASK))
			v = hn;
			break;
	}

	if (v) {
#if DEBUG_PAGE_ALLOC
		DEBUG_MSG("deleting node %p which contained page %p", v, p);
#endif
		hlist_del(v);
	}
}


struct page* dde_page_lookup(unsigned long va)
{
	unsigned int hashval = VIRT_TO_PAGEHASH(va);

	struct hlist_node *hn = NULL;
	struct hlist_head *h  = &dde_page_cache[hashval];
	page_cache_entry *e   = NULL;

	hlist_for_each_entry(e, hn, h, list) {
		if ((unsigned long)e->page->virtual == (va & PAGE_MASK))
			return e->page;
	}

	return NULL;
}


struct page * __alloc_pages_internal(gfp_t gfp_mask, unsigned int order,
                                     struct zonelist *zonelist, nodemask_t *nm)
{
	/* XXX: In fact, according to order, we should have one struct page
	 *      for every page, not only for the first one.
	 */
	struct page *ret = kmalloc(sizeof(*ret), GFP_KERNEL);
	
	ret->virtual = (void *)__get_free_pages(gfp_mask, order);
	dde_page_cache_add(ret);

	return ret;
}


unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	ddekit_log(DEBUG_PAGE_ALLOC, "gfp_mask=%x order=%d (%d bytes)",
	           gfp_mask, order, PAGE_SIZE << order);

	Assert(gfp_mask != GFP_DMA);
	void *p = ddekit_large_malloc(PAGE_SIZE << order);

	return (unsigned long)p;
}

unsigned int nr_free_buffer_pages(void)
{
	WARN_UNIMPL;
	return 1024;
}

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	unsigned long p = __get_free_pages(gfp_mask, 0);

	if (p) memset((void *)p, 0, PAGE_SIZE);

	return (unsigned long)p;
}


void free_hot_page(struct page *page)
{
	WARN_UNIMPL;
}

/* 
 * XXX: If alloc_pages() gets fixed to allocate a page struct per page,
 *      this needs to be adapted, too.
 */
void __free_pages(struct page *page, unsigned int order)
{
	free_pages((unsigned long)page->virtual, order);
	dde_page_cache_remove(page);
}

void __pagevec_free(struct pagevec *pvec)
{
	WARN_UNIMPL;
}

int get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
                   unsigned long start, int len, int write, int force,
                   struct page **pages, struct vm_area_struct **vmas)
{
	WARN_UNIMPL;
	return 0;
}

/**
 * ...
 *
 * XXX order may be larger than allocation at 'addr' - it may comprise several
 * allocation via __get_free_pages()!
 */
void free_pages(unsigned long addr, unsigned int order)
{
	ddekit_log(DEBUG_PAGE_ALLOC, "addr=%p order=%d", (void *)addr, order);

	ddekit_large_free((void *)addr);
}


unsigned long __pa(volatile const void *addr)
{
	return ddekit_pgtab_get_physaddr((void*)addr);
}

void *__va(unsigned long addr)
{
	return (void*)ddekit_pgtab_get_virtaddr((ddekit_addr_t) addr);
}


int set_page_dirty_lock(struct page *page)
{
	WARN_UNIMPL;
	return 0;
}

int hashdist = HASHDIST_DEFAULT;
/*
 * basically copied from linux/mm/page_alloc.c
 */
void *__init alloc_large_system_hash(const char *tablename,
                                     unsigned long bucketsize,
                                     unsigned long numentries,
                                     int scale,
                                     int flags,
                                     unsigned int *_hash_shift,
                                     unsigned int *_hash_mask,
                                     unsigned long limit)
{
	void * table = NULL;
	unsigned long log2qty;
	unsigned long size;

	if (numentries == 0)
		numentries = 1024;

	log2qty = ilog2(numentries);
	size = bucketsize << log2qty;

	do {
		unsigned long order;
		for (order = 0; ((1UL << order) << PAGE_SHIFT) < size; order++);
			table = (void*) __get_free_pages(GFP_ATOMIC, order);
	} while (!table && size > PAGE_SIZE && --log2qty);

	if (!table)
		panic("Failed to allocate %s hash table\n", tablename);

	printk("%s hash table entries: %d (order: %d, %lu bytes)\n",
	       tablename,
	       (1U << log2qty),
	       ilog2(size) - PAGE_SHIFT,
	       size);

	if (_hash_shift)
		*_hash_shift = log2qty;
	if (_hash_mask)
		*_hash_mask = (1 << log2qty) - 1;

	return table;
}


static void __init dde_page_cache_init(void)
{
	printk("Initializing DDE page cache\n");
	int i=0;

	for (i; i < DDE_PAGE_CACHE_SIZE; ++i)
		INIT_HLIST_HEAD(&dde_page_cache[i]);
}

core_initcall(dde_page_cache_init);
