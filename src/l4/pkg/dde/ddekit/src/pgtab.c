/*
 * \brief   Virtual page-table facility
 *
 * This implementation uses l4rm (especially the AVL tree and userptr) to
 * manage virt->phys mappings. Each mapping region is represented by one
 * pgtab_object that is kept in the l4rm region userptr.
 *
 * For this to work, dataspaces must be attached to l4rm regions!
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

#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/sys/kdebug.h>
#include <pthread-l4.h>

#include "config.h"


/**
 * "Page-table" object
 */
struct pgtab_object
{
	l4_addr_t va;    /* virtual start address */
	l4_addr_t pa;    /* physical start address */

	/* FIXME reconsider the following members */
	l4_size_t size;
	unsigned  type;  /* pgtab region type */
	
	struct pgtab_object * next;
	struct pgtab_object * prev;
};

/**
 * pa_list_head of page-table object list (for get_virtaddr())
 */
static struct pgtab_object pa_list_head =
{
	.va = 0,
	.pa = 0,
	.size = 0,
	.type = 0,
	.next = &pa_list_head,
	.prev = &pa_list_head
};


static void  __attribute__((used)) dump_pgtab_list(void)
{
	struct pgtab_object *p = pa_list_head.next;

	ddekit_printf("PA LIST DUMP\n");
	for ( ; p != &pa_list_head; p = p->next)
	{
		ddekit_printf("\t%p -> %p -> %p\n", p->prev, p, p->next);
	}
	enter_kdebug("dump");
}

static pthread_mutex_t pa_list_lock;

void ddekit_pgtab_init(void);
void ddekit_pgtab_init(void)
{
	int r = pthread_mutex_init(&pa_list_lock, NULL);
	if (r) {
		ddekit_printf("Error initializing pgtab mutex: %d\n", r);
		ddekit_panic("error initializing pgtab mutex");
	}
}


static struct pgtab_object *__find(l4_addr_t virt)
{
	struct pgtab_object *p = NULL;

	pthread_mutex_lock(&pa_list_lock);
	for (p = pa_list_head.next; p != &pa_list_head; p = p->next)
	{
		if (virt >= p->va && virt < p->va + p->size)
			break;
	}
	pthread_mutex_unlock(&pa_list_lock);

	return p == &pa_list_head ? NULL : p;
}

/*****************************
 ** Page-table facility API **
 *****************************/

/**
 * Get physical address for virtual address
 *
 * \param virtual  virtual address
 * \return physical address or 0
 */
ddekit_addr_t ddekit_pgtab_get_physaddr(const void *virt)
{
	/* find pgtab object */
	struct pgtab_object *p = __find((l4_addr_t)virt);
	if (!p) {
		/* XXX this is verbose */
		ddekit_debug("%s: no virt->phys mapping for virtual address %p\n", __func__, virt);
		return 0;
	}

	/* return virt->phys mapping */
	l4_size_t offset = (l4_addr_t) virt - p->va;

	return p->pa + offset;
}

/**
 * Get virt address for physical address
 *
 * \param physical  physical address
 * \return virtual address or 0
 */
ddekit_addr_t ddekit_pgtab_get_virtaddr(const ddekit_addr_t physical)
{
	/* find pgtab object */
	struct pgtab_object *p;
	ddekit_addr_t retval = 0;
	
	/* find phys->virt mapping */
	pthread_mutex_lock(&pa_list_lock);
	for (p = pa_list_head.next ; p != &pa_list_head ; p = p->next) {
		if (p->pa <= (l4_addr_t)physical && 
		    (l4_addr_t)physical < p->pa + p->size) {
			l4_size_t offset = (l4_addr_t) physical - p->pa;
			retval = p->va + offset;
			break;
		}
	}
	pthread_mutex_unlock(&pa_list_lock);

	if (!retval)
		ddekit_debug("%s: no phys->virt mapping for physical address %p", __func__, (void*)physical);

	return retval;
}



int ddekit_pgtab_get_type(const void *virt)
{
	/* find pgtab object */
	struct pgtab_object *p = __find((l4_addr_t)virt);
	if (!p) {
		/* XXX this is verbose */
		ddekit_debug("%s: no virt->phys mapping for %p", __func__, virt);
		return -1;
	}

	return p->type;
}


int ddekit_pgtab_get_size(const void *virt)
{
	/* find pgtab object */
	struct pgtab_object *p = __find((l4_addr_t)virt);
	if (!p) {
		/* XXX this is verbose */
		ddekit_debug("%s: no virt->phys mapping for %p", __func__, virt);
		return -1;
	}

	return p->size;
}


/**
 * Clear virtual->physical mapping for VM region
 *
 * \param virtual   virtual start address for region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_clear_region(void *virt, int type __attribute__((unused)))
{
#if 0
	ddekit_printf("before %s\n", __func__);
	dump_pgtab_list();
#endif

	struct pgtab_object *p = __find((l4_addr_t)virt);
	if (!p) {
		/* XXX this is verbose */
		ddekit_debug("%s: no virt->phys mapping for %p\n", __func__, virt);
		return;
	}


	pthread_mutex_lock(&pa_list_lock);
	/* remove pgtab object from list */
	p->next->prev= p->prev;
	p->prev->next= p->next;
	pthread_mutex_unlock(&pa_list_lock);
	
	/* free pgtab object */
	ddekit_simple_free(p);

#if 0
	ddekit_printf("after %s\n", __func__);
	dump_pgtab_list();
#endif
}


/**
 * Set virtual->physical mapping for VM region
 *
 * \param virtual   virtual start address for region
 * \param physical  physical start address for region
 * \param pages     number of pages in region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_set_region(void *virt, ddekit_addr_t phys, int pages, int type)
{
	/* allocate pgtab object */
	struct pgtab_object *p = ddekit_simple_malloc(sizeof(*p));
	if (!p) {
		ddekit_printf("ddekit heap exhausted\n");
		return;
	}
	/* initialize pgtab object */
	p->va   = l4_trunc_page((l4_addr_t)virt);
	p->pa   = l4_trunc_page(phys);
	p->size = pages * L4_PAGESIZE;
	p->type = type;

	pthread_mutex_lock(&pa_list_lock);

	p->next = pa_list_head.next;
	p->prev = &pa_list_head;
	p->next->prev = p;
	pa_list_head.next = p;

	pthread_mutex_unlock(&pa_list_lock);

#if 0
	ddekit_printf("after %s\n", __func__);
	dump_pgtab_list();
#endif
}

void ddekit_pgtab_set_region_with_size(void *virt, ddekit_addr_t phys, int size, int type)
{
	int p = l4_round_page(size);
	p >>= L4_PAGESHIFT;
//	ddekit_printf("%s: virt %p, phys %p, pages %d\n", __func__, virt, phys, p);
	ddekit_pgtab_set_region(virt, phys, p, type);
}

