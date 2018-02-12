/*
 * \brief   DOpE cache handling module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module provides the needed functionality
 * to handle caches.  Although the caches can be
 * used for every kind of  data the primary  use
 * of this module is the management of image and
 * font caches.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "cache.h"

struct cache_elem {
	void  *data;               /* cached data block */
	s32    size;               /* size of data block */
	s32    ident;              /* data block identifier */
	void (*destroy)(void *);   /* data block destroy function */
};

struct cache {
	s32 max_entries;                /* size of ring buffer */
	s32 max_size;                   /* max amount of cached data */
	s32 curr_size;                  /* current amount of cached data */
	s32 idxtokill;                  /* 'oldest' ring buffer index */
	s32 idxtoadd;                   /* ring buffer index for new element */
	struct cache_elem *elem; /* pointer to ring buffer */
};

int init_cache(struct dope_services *d);



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** CREATE NEW CACHE ***/
static CACHE *create(s32 max_entries,s32 max_size) {
	struct cache *c;

	/* get memory for cache struct and the ring buffer */
	c = (struct cache *)zalloc(sizeof(struct cache)
	                         + sizeof(struct cache_elem)*max_entries);
	if (!c) {
		INFO(printf("Cache(create): out of memory\n");)
		return NULL;
	}

	/* set values in cache struct */
	c->max_entries = max_entries;
	c->max_size    = max_size;
	c->elem = (struct cache_elem *)((long)c + sizeof(struct cache));

	return c;
}


/*** REMOVE ELEMENT FROM CACHE ***/
static void remove_elem(CACHE *cache,s32 index) {
	struct cache_elem *e;
	if (!cache) {
		INFO(printf("Cache(remove_elem): cache == NULL\n");)
		return;
	}
	if (index >= cache->max_entries) {
		INFO(printf("Cache(remove_elem): index out of range\n");)
		return;
	}
	e=cache->elem + index;
	INFO(printf("Cache(remove_element): removing element %u\n",index);)
	if (e->data) {
		if (e->destroy) e->destroy(e->data);
		else free(e->data);
		cache->curr_size -= e->size;
		e->data=NULL;
	}
}


/*** DESTROY CACHE ***/
static void destroy(CACHE *cache) {
	s32 i;
	if (!cache) return;

	/* free cache entries */
	for (i=0;i<cache->max_entries;i++) remove_elem(cache,i);

	/* free cache struct itself */
	free(cache);
}


static void reduce_cachesize(CACHE *cache,long needed_size) {
	if (!cache) return;

	INFO(printf("Cache(reduce_cachesize): old size is %u\n",cache->curr_size);)
	while (cache->curr_size > needed_size) {
		remove_elem(cache,cache->idxtokill);
		cache->idxtokill = (cache->idxtokill + 1)%(cache->max_entries);
	}
	INFO(printf("Cache(reduce_cachesize): new size is %u\n",cache->curr_size);)
}


/*** INSERT ELEMENT INTO CACHE ***/
/* parameters:                                                               */
/*   elem     - data block to add to cache                                   */
/*   elemsize - size of data block (needed to determine when element must be */
/*              removed)                                                     */
/*   ident    - identifier for this element (needed to determine if the      */
/*              element did not got overwritten by another                   */
/*   destroy  - function that is called when an element needs to be          */
/*              destroyed                                                    */
/* returns:                                                                  */
/*   index to cache element. The cached element can be accessed later using  */
/*   the function get_element with this index and the identifier as args.    */
/**/
static s32 add_elem(CACHE *cache,void *elem,s32 elemsize,s32 ident,void (*destroy)(void *)) {
	struct cache_elem *e;
	s32 new_idx;

	if (!cache) return -1;

	/* check if cache grows bigger that its maximum size */
	if (cache->curr_size + elemsize > cache->max_size) {
		reduce_cachesize(cache,cache->max_size - elemsize);
	}

	new_idx = cache->idxtoadd;
	e=cache->elem + new_idx;
	INFO(printf("Cache(add_elem): add element at index %u\n",new_idx);)

	/* gets another cache element overwritten? */
	if (e->data) {
		INFO(printf("Cache(add_elem): old element gets overwritten\n");)

		/* remove old element */
		remove_elem(cache,new_idx);
		cache->idxtokill = (cache->idxtokill + 1)%(cache->max_entries);
	}

	e->data=elem;
	e->size=elemsize;
	e->ident=ident;
	e->destroy=destroy;

	cache->curr_size += elemsize;
	cache->idxtoadd = (cache->idxtoadd + 1)%(cache->max_entries);

	return new_idx;
}


/*** GET CACHED DATA ***/
static void *get_elem(struct cache *cache,s32 index,s32 ident) {

	if (!cache) {
		INFO(printf("Cache(get_elem): cache == NULL\n");)
		return NULL;
	}
	if (index >= cache->max_entries) {
		INFO(printf("Cache(get_elem): index %u out of range\n",index);)
		return NULL;
	}
	if ((cache->elem + index)->ident == ident) {
		return (cache->elem + index)->data;
	}
	INFO(printf("Cache(get_elem): element %u is not cached anymore\n",index);)
	return NULL;
}



/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct cache_services services = {
	create,
	destroy,
	add_elem,
	get_elem,
	remove_elem
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_cache(struct dope_services *d) {
	d->register_module("Cache 1.0",&services);
	return 1;
}
