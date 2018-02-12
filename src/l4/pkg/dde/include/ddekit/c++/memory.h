/*
 * \brief   Memory subsystem
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 * \date    2006-11-03
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */


#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

namespace DDEKit
{
/*******************
 ** Slab facility **
 *******************/

class Slab
{
	public:
		virtual ~Slab() { };

		virtual void set_data(l4_addr_t data) = 0;
		virtual l4_addr_t get_data() = 0;
		virtual l4_addr_t alloc() = 0;
		virtual void free(l4_addr_t ptr) = 0;
		virtual bool contiguous() = 0;

		static Slab* create(int const size, bool const contiguous);

	protected:
		explicit Slab() { }
		Slab(const Slab&) { }
		Slab& operator=(const Slab&) { return *this; }
};

void init_mm(void);

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
void *large_malloc(int const size);

/**
 * Free large memory block
 *
 * \param p  pointer to memory block
 */
void  large_free(void *p);

/** FIXME
 * contig_malloc() is the lowest-level allocator interface one could implement.
 * we should consider to provide vmalloc() too. */
void *contig_malloc(
		unsigned long const size,
        unsigned long const low, unsigned long const high,
        unsigned long const alignment, unsigned long const boundary
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
void *simple_malloc(unsigned const size);

/**
 * Free memory block via simple allocator
 *
 * \param p  pointer to memory block
 */
void simple_free(void *p);

}
