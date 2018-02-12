/*
 * \brief   Simple allocator implementation
 *
 * This simple allocator provides malloc() and free() using dm_mem dataspaces
 * as backing store. The actual list-based allocator implementation is from
 * l4util resp. Fiasco.
 *
 * For large allocations and slab-based OS-specific allocators
 * ddekit_large_malloc and ddekit_slab_*() should be used. The blocks
 * allocated via this allocator CANNOT be used for DMA or other device
 * operations, i.e., there exists no virt->phys mapping.
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

/*
 * FIXME check thread-safety and add locks where appropriate
 */

#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/panic.h>

#include <l4/sys/consts.h>
#include <l4/util/list_alloc.h>
#include <l4/re/dataspace>
#include <l4/re/rm>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <pthread.h>


/* configuration */
#define ALLOC_SIZE     (4 * L4_PAGESIZE)

/* malloc pool is a list allocator */
static l4la_free_t *malloc_pool = NULL;
static pthread_mutex_t malloc_mtx = PTHREAD_MUTEX_INITIALIZER;


/**
 * Allocate memory block via simple allocator
 *
 * \param size  block size
 * \return pointer to new memory block
 *
 * The blocks allocated via this allocator CANNOT be used for DMA or other
 * device operations, i.e., there exists no virt->phys mapping.
 *
 * Each chunk stores its size in the first word for free() to work.
 */
EXTERN_C void * ddekit_simple_malloc(unsigned size)
{
	pthread_mutex_lock(&malloc_mtx);
	/* we store chunk size in the first word of the chunk */
	size += sizeof(unsigned);

	/* try to allocate */
	unsigned *p = static_cast<unsigned *>(l4la_alloc(&malloc_pool, size, 0));

	/* fill pool if allocation fails */
	if (!p) {
		/* size of allocated dataspace is at least ALLOC_SIZE */
		unsigned ds_size = l4_round_page(size);
		ds_size = (ds_size > ALLOC_SIZE) ? ds_size : ALLOC_SIZE;

		L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
		if (!ds.is_valid())
			goto err_out;

		void *res = 0;
		int err = L4Re::Env::env()->mem_alloc()->alloc(ds_size, ds);
		if (err < 0)
			goto err_out;

		err = L4Re::Env::env()->rm()->attach(&res, ds_size, L4Re::Rm::Search_addr, L4::Ipc::make_cap_rw(ds), 0);
		if (err < 0)
			goto err_out;

		if (!res) 
			goto err_out;

		l4la_free(&malloc_pool, res, ds_size);
		p = static_cast<unsigned *>(l4la_alloc(&malloc_pool, size, 0));
	}

	/* store chunk size */
	if (p) {
		*p = size;
		p++;
	}

err_out:
        // FIXME: goto is error prone in C++
        // FIXME: missing nearly all free operations here
	pthread_mutex_unlock(&malloc_mtx);

	return p;
}


/**
 * Free memory block via simple allocator
 *
 * \param p  pointer to memory block
 */
EXTERN_C void ddekit_simple_free(void *p)
{
	unsigned *chunk = (unsigned *)p - 1;
	if (p)
	{
		pthread_mutex_lock(&malloc_mtx);
		l4la_free(&malloc_pool, chunk, *chunk);
		pthread_mutex_unlock(&malloc_mtx);
	}
}
