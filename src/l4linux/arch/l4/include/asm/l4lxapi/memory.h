/*
 * This header file defines the L4Linux internal interface for
 * memory handling functionality. All APIs have to implement it.
 *
 */
#ifndef __ASM_L4__L4LXAPI__MEMORY_H__
#define __ASM_L4__L4LXAPI__MEMORY_H__

/**
 * \defgroup memory Memory handling functions.
 * \ingroup l4lxapi
 */

/**
 * \brief Map a page into the virtual address space.
 * \ingroup memory
 *
 * \param	address		Virtual address.
 * \param	page		Physical address.
 * \param	pte		Pte (source address).
 *
 * \return	0 on success, != 0 on error
 */
int l4lx_memory_map_virtual_page(unsigned long address, pte_t pte);

int l4lx_memory_map_virtual_range(unsigned long address, unsigned long size,
                                  unsigned long page, int map_rw);

/**
 * \brief Unmap a page from the virtual address space.
 * \ingroup memory
 *
 * \param	address		Virtual adress.
 *
 * \return	0 on success, != 0 on error
 */
int l4lx_memory_unmap_virtual_page(unsigned long address);

#endif /* ! __ASM_L4__L4LXAPI__MEMORY_H__ */
