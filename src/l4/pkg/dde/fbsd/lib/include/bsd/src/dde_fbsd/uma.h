#ifndef _dde_fbsd_uma_h
#define _dde_fbsd_uma_h

#include <l4/dde/ddekit/memory.h>

struct uma_zone {
	char *name;
	ddekit_lock_t lock;
	size_t size;
	uma_ctor ctor;
	uma_dtor dtor;
	uma_init init;
	uma_fini fini;
	int align;
	u_int16_t flags;
	struct ddekit_slab *slab;
	// statistics
	u_int64_t allocs;  // number of alloc calls
	u_int64_t alloced; // number of alloced items
};

uma_zone_t uma_getzone(void *objp);

void *bsd_large_malloc(int size, int flags);
void  bsd_large_free(void *p);

#endif
