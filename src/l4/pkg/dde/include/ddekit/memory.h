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

/*
 * \brief   Memory subsystem
 */

#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/*******************
 ** Slab facility **
 *******************/

struct ddekit_slab;

/**
 * Store user pointer in slab cache
 *
 * \param slab  pointer to slab cache
 * \param data  user pointer
 */
void ddekit_slab_set_data(struct ddekit_slab * slab, void *data);

/**
 * Read user pointer from slab cache
 *
 * \param slab  pointer to slab cache
 *
 * \return stored user pointer or 0
 */
void *ddekit_slab_get_data(struct ddekit_slab * slab);

/**
 * Allocate slab in slab cache
 *
 * \param slab  pointer to slab cache
 *
 * \return pointer to allocated slab
 */
void *ddekit_slab_alloc(struct ddekit_slab * slab);

/**
 * Deallocate slab in slab cache
 *
 * \param slab  pointer to slab cache
 * \param objp  pointer to allocated slab
 */
void ddekit_slab_free(struct ddekit_slab * slab, void *objp);

/**
 * Setup page cache for all slabs
 *
 * \param pages  maximal number of memory pages
 *
 * If 'pages' is too low, memory pages may be given back to the memory server
 * (dm_phys) and just to be allocated again later. This hits performance (but
 * saves memory). Increase 'pages' to avoid this thrashing-like effect.
 *
 * If the maximal number of unused pages is exceeded, subsequent deallocation
 * will be freed at the memory server. This page cache caches pages from all
 * slabs.
 */
void ddekit_slab_setup_page_cache(unsigned pages);

/**
 * Destroy slab cache
 *
 * \param slab  pointer to slab cache structure
 */
void ddekit_slab_destroy(struct ddekit_slab * slab);

/**
 * Initialize slab cache
 *
 * \param size          size of cache objects
 * \param contiguous    make this slab use physically contiguous memory
 *
 * \return pointer to new slab cache or 0 on error
 */
struct ddekit_slab * ddekit_slab_init(unsigned size, int contiguous);


/**********************
 ** Memory allocator **
 **********************/

/**
 * Allocate large memory block
 *
 * \param size  block size
 * \return pointer to new memory block
 *
 * Allocations via this allocator may be slow (because memory servers are
 * involved) and should be used only for large (i.e., > page size) blocks. If
 * allocations/deallocations are relatively dynamic this may not be what you
 * want.
 *
 * Allocated blocks have valid virt->phys mappings and are physically
 * contiguous.
 */
void *ddekit_large_malloc(int size);

/**
 * Free large memory block
 *
 * \param p  pointer to memory block
 */
void  ddekit_large_free(void *p);

/** FIXME
 * contig_malloc() is the lowest-level allocator interface one could implement.
 * we should consider to provide vmalloc() too. */
void *ddekit_contig_malloc(
		unsigned long size,
        unsigned long low, unsigned long high,
        unsigned long alignment, unsigned long boundary
);


/*****************************
 ** Simple memory allocator **
 *****************************/

/**
 * Allocate memory block via simple allocator
 *
 * \param size  block size
 * \return pointer to new memory block
 *
 * The blocks allocated via this allocator CANNOT be used for DMA or other
 * device operations, i.e., there exists no virt->phys mapping.
 */
void *ddekit_simple_malloc(unsigned size);

/**
 * Free memory block via simple allocator
 *
 * \param p  pointer to memory block
 */
void ddekit_simple_free(void *p);

EXTERN_C_END
