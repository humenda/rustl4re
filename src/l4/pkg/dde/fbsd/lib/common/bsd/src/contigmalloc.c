/**
 * Implememtation of contigmalloc() and contigfree().
 * Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <machine/param.h>

#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/panic.h>

void *
contigmalloc(
		unsigned long size,	 /* should be size_t here and for malloc() */
		struct malloc_type *type,
		int flags,
		vm_paddr_t low,
		vm_paddr_t high,
		unsigned long alignment,
		unsigned long boundary)
{
	void *res;

	res = ddekit_contig_malloc(size, low, high, alignment, boundary);

	if (res) {
		// announce allocation
		malloc_type_allocated(type, res ? ddekit_pgtab_get_size(res) : 0);

		if (flags & M_ZERO)
			bzero(res, size);
	} else {
		if (flags & M_WAITOK)
			ddekit_panic("M_WAITOK set but out of memory");
	}

	return res;
}

void
contigfree(void *addr, unsigned long size, struct malloc_type *memtype) {
	// release
	ddekit_large_free(addr);

	// announce release
	malloc_type_freed(memtype, ddekit_pgtab_get_size(addr));
}
