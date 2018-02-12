/*
 * \brief   Virtual page-table facility
 *
 * Implementation of page tables for saving virt->phys assignments.
 *
 * FIXME: This works for 32-bit architectures only! (Mostly because of pgtab.h.)
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

#include <l4/sys/l4int.h>
#include <l4/sys/consts.h>
#include <l4/util/macros.h>
#include <l4/dm_mem/dm_mem.h>
#include <l4/log/l4log.h>


/**********************************
 ** Page-table utility functions **
 **********************************/

/* some useful consts */
enum
{
	PDIR_SHIFT   = L4_SUPERPAGESHIFT,  /* readable alias */
	PDIR_MASK    = L4_SUPERPAGEMASK,   /* readable alias */
	PDIR_ENTRIES = (1 << (L4_MWORD_BITS - PDIR_SHIFT)),
	PTAB_ENTRIES = (1 << (PDIR_SHIFT - L4_PAGESHIFT)),
	PTAB_SIZE    = (sizeof(void*) * PTAB_ENTRIES)
};

/**
 * Page table entry.
 * bit 00-01: type, 0->largemalloc 1->slab page 2->contigmalloc (PTE_TYPE_*)
 * bit 02-11: size in number of pages
 * bit 12-31: physical page address
 */
typedef union
{
	unsigned compact;
	struct
	{
		unsigned type  :  2;
		unsigned pages : 10;
		unsigned phys  : 20;
	} components;
} ddekit_pte;

/**
 * Calculate offset of address in page directory - page-table number
 */
static inline unsigned pt_num(l4_addr_t addr) {
	return addr >> PDIR_SHIFT; }


/**
 * Calculate offset of address in page table - page number
 */
static inline unsigned pg_num(l4_addr_t addr) {
	return (addr & ~PDIR_MASK) >> L4_PAGESHIFT; }


/* page directory */
static ddekit_pte *page_dir[PDIR_ENTRIES];


/**
 * Get page-table entry
 */
static inline ddekit_pte ddekit_get_pte(void *p)
{
	l4_addr_t addr = (l4_mword_t)p;
	ddekit_pte *tab;

	tab = page_dir[pt_num(addr)];
	if (!tab) return (ddekit_pte) 0U;

	return tab[pg_num(addr)];
}


/**
 * Set page-table entry
 */
static inline void ddekit_set_pte(void *p, ddekit_pte pte)
{
	l4_addr_t addr = (l4_addr_t) p;
	ddekit_pte *tab;

	tab = page_dir[pt_num(addr)];
	if (!tab) {
		/* create new page table */
		tab = (ddekit_pte *)
		      l4dm_mem_allocate_named(PTAB_SIZE, L4DM_CONTIGUOUS|L4DM_PINNED|L4RM_MAP,
		                              "ddekit ptab");
		Assert(tab);

		memset(tab, 0, PTAB_SIZE);
		page_dir[pt_num(addr)] = tab;

		if (1) {
			l4_addr_t a = l4_trunc_superpage(addr);
			LOG("created page table for range [0x%08lx,0x%08lx) @ %p\n",
			     a, a + L4_SUPERPAGESIZE, tab);
		}

	}

	tab[pg_num(addr)] = pte;
}


/*****************************
 ** Page-table facility API **
 *****************************/

/**
 * Set virtual->physical mapping for VM region
 *
 * \param virtual   virtual start address for region
 * \param physical  physical start address for region
 * \param pages     number of pages in region
 * \param type      pte type for region
 */
void ddekit_pte_set_region(void *virtual, ddekit_addr_t physical, int pages, int type)
{
	ddekit_pte new;

#ifdef INVARIANTS
	ddekit_pte pte;
	/* assert pte not set yet */
	pte = ddekit_get_pte(virtual);
	LOGd(pte.compact, "first pte already set! pte=0x%04x", pte.compact);
#endif

	/* build pte */
	new.components.type  = type;
	new.components.pages = pages;
	new.components.phys  = physical >> L4_PAGESHIFT;

	/* set first pte */
	ddekit_set_pte(virtual, new);

	/* continuation pte-s don't have pages set */
	new.components.pages = 0;

	/* set continuation pte-s */
	for (pages--; pages; pages--) {
		/* prepare continuation pte */
		virtual += L4_PAGESIZE;
		new.components.phys++;

#ifdef INVARIANTS
		/* assert pte not set yet */
		pte = ddekit_get_pte(virtual);
		LOGd(pte.compact, "continuation pte already set! pte=0x%04x", pte.compact);
#endif

		/* set continuation pte */
		ddekit_set_pte(virtual, new);
	}
}


/**
 * Clear virtual->physical mapping for VM region
 *
 * \param virtual   virtual start address for region
 * \param type      pte type for region
 *
 * XXX unused page-tables not free'd
 */
void ddekit_pte_clear_region(void *virtual, int type)
{
	ddekit_pte pte;
	int pages = 0;
	pte = ddekit_get_pte(virtual);
	/* get number of pages from pte */
	pages = pte.components.pages;
	/* assert pages != 0 */
	if (pages==0) {
		LOG("continuation pte found at base");
		return;
	}
	/* assert pte set and of correct type */
#ifdef INVARIANTS
	LOGd(! pte.compact, "pte is already clear");
	LOGd(pte.components.type!=type, "pte of wrong type");
#endif
	/* clear first pte */
	ddekit_set_pte(virtual, (ddekit_pte) 0U);
	/* clear continuation pte-s */
	for (pages--; pages; pages--) {
		virtual += L4_PAGESIZE;

#ifdef INVARIANTS
		/* assert pte set and of correct type */
		pte = ddekit_get_pte(virtual);
		if (! pte.compact) {
			LOG("pte is already clear");
			break;
		}
		if (pte.components.type!=type) {
			LOG("pte of wrong type");
			break;
		}
		if (pte.components.pages) {
			LOG("unexpected non-continuation pte found");
			break;
		}
#endif

		ddekit_set_pte(virtual, (ddekit_pte) 0U);
	}
}

