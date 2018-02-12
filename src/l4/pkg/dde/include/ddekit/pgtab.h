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
 * \brief   Virtual page-table facility
 */

#pragma once

#include <l4/sys/compiler.h>
#include <l4/dde/ddekit/types.h>

EXTERN_C_BEGIN

/* FIXME Region types may be defined by pgtab users. Do we really need them
 * here? */
enum ddekit_pgtab_type
{
	PTE_TYPE_OTHER, PTE_TYPE_LARGE, PTE_TYPE_UMA, PTE_TYPE_CONTIG
};


/**
 * Set virt->phys mapping for VM region
 *
 * \param virt   virt start address for region
 * \param phys  phys start address for region
 * \param pages     number of pages in region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_set_region(void *virt, ddekit_addr_t phys, int pages, int type);


/**
 * Set virt->phys mapping for VM region given a specific size in bytes.
 *
 * Internally, DDEKit manages regions with pages. However, DDEs do not need to tangle
 * with the underlying mechanism and therefore can use this function that takes care
 * of translating a size to an amount of pages.
 */
void ddekit_pgtab_set_region_with_size(void *virt, ddekit_addr_t phys, int size, int type);


/**
 * Clear virt->phys mapping for VM region
 *
 * \param virt   virt start address for region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_clear_region(void *virt, int type);

/**
 * Get phys address for virt address
 *
 * \param virt  virt address
 *
 * \return phys address
 */
ddekit_addr_t ddekit_pgtab_get_physaddr(const void *virt);

/**
 * Get virt address for phys address
 *
 * \param phys  phys address
 *
 * \return virt address
 */
ddekit_addr_t ddekit_pgtab_get_virtaddr(const ddekit_addr_t phys);

/**
 * Get type of VM region.
 *
 * \param virt   virt address

 * \return VM region type
 */
int ddekit_pgtab_get_type(const void *virt);

/**
 * Get size of VM region.
 *
 * \param virt   virt address
 *
 * \return VM region size (in bytes)
 */
int ddekit_pgtab_get_size(const void *virt);

EXTERN_C_END
