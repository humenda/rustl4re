/**
 * UMA implementation using dde_slabs. Written from scratch.
 * Inspired by FreeBSD's implementation.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <vm/uma.h>

#include <dde_fbsd/uma.h>
#include <l4/dde/ddekit/debug.h>
#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>

static struct ddekit_slab *zone_cache = NULL;

uma_zone_t uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
                        uma_init uminit, uma_fini fini, int align,
                        u_int16_t flags)
{
	uma_zone_t zone;
	size_t realsize;

	// alloc new zone
	zone = (uma_zone_t) ddekit_slab_alloc(zone_cache);
	if (!zone) {
		printf("ERROR: could not allocate new zone, this could be fatal\n");
		ddekit_debug("fatal error?");
		return NULL;
	}

	// initialize trivial members
	zone->name    = name;
	ddekit_lock_init(&zone->lock);
	zone->size    = size;
	zone->ctor    = ctor;
	zone->dtor    = dtor;
	zone->init    = uminit;
	zone->fini    = fini;
	zone->align   = align;
	zone->flags   = flags;
	zone->allocs  = 0;
	zone->alloced = 0;

	// adjust size to match alignment
	if (size & align) {
		realsize = (size&~align) + align+1 ;
	} else {
		realsize = size;
	}

	// initialize zone's slab cache
	zone->slab = ddekit_slab_init(realsize, 1);
	if (!zone->slab) {
		printf("ERROR: ddekit_slab_init failed, this could be fatal\n");
		ddekit_debug("fatal error?");
		ddekit_slab_free(zone_cache, zone);
		return NULL;
	} 

	// set pointer to wrapping uma zone
	ddekit_slab_set_data(zone->slab, zone);

	return zone;
}

void uma_zdestroy(uma_zone_t zone) {
	if (zone->alloced) {
		printf("WARNING: destroying zone with %lld items still allocated\n", zone->alloced);
	}
	ddekit_lock_deinit(&zone->lock);
	ddekit_slab_destroy(zone->slab);
	ddekit_slab_free(zone_cache, zone);
}

uma_zone_t uma_getzone(void *objp) {
#if 0
	struct ddekit_slab *sp;
	sp = ddekit_slab_get_slab(objp);
	return (uma_zone_t) ddekit_slab_get_data(sp);
#else
	ddekit_printf("WARNING: There is no way to retrieve the slab belonging to an obj\n");
	ddekit_printf("pointer. Implement it, if you get along this way.\n");
	return (uma_zone_t)NULL;
#endif
}

void *uma_zalloc_arg(uma_zone_t zone, void *arg, int flags) {
	void *res=NULL;
	
	ddekit_lock_lock(&zone->lock);
	
	zone->allocs++;

	res = ddekit_slab_alloc(zone->slab);
	
	if (res) {
		zone->alloced++;
		// unlock zone before doing time consuming work
		ddekit_lock_unlock(&zone->lock);

		if (zone->init)
			zone->init(res, zone->size, flags);
		if (zone->ctor)
			zone->ctor(res, zone->size, arg, flags);
		if ((zone->flags & UMA_ZONE_ZINIT) | (flags & M_ZERO))
			bzero(res, zone->size);
	} else {
		ddekit_lock_unlock(&zone->lock);
	}

	if (! res && (flags & M_WAITOK)) {
		printf("ERROR: ddekit_slab_alloc failed, but M_WAITOK set, so i cannot return ZERO, this could be fatal\n");
		ddekit_debug("fatal error?");
	}
	
	return res;
}

void uma_zfree_arg(uma_zone_t zone, void *item, void *arg) {
	if (zone->flags & UMA_ZONE_NOFREE) {
		printf("WARNING: i should not free (UMA_ZONE_NOFREE) elements of this zone (\"%s\")\n", zone->name);
		return;
	}

	if (zone->dtor)
		zone->dtor(item, zone->size, arg);
	if (zone->fini)
		zone->fini(item, zone->size);
	
	ddekit_lock_lock(&zone->lock);
	ddekit_slab_free(zone->slab, item);
	zone->alloced--;
	ddekit_lock_unlock(&zone->lock);
}

void *bsd_large_malloc(int size, int flags) {
	void *res;

	res = ddekit_large_malloc(size);
	if (res) {
		if (flags & M_ZERO)
			bzero(res, size);
	} else {
		if (flags & M_WAITOK)
			ddekit_panic("M_WAITOK given but out of memory");
	}

	return res;
}

void bsd_large_free(void *p) {
	ddekit_large_free(p);
}

static void uma_ddeslab_init(void *dummy __unused) {
	zone_cache = ddekit_slab_init(sizeof(struct uma_zone), 1);
	if (!zone_cache) {
		printf("ERROR: slab cache initialization failed, this could be fatal\n");
		ddekit_debug("slab init failed");
	}
}

SYSINIT(dde_uma, SI_DDE_UMA, SI_ORDER_FIRST, uma_ddeslab_init, NULL)
