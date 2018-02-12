#pragma once

#include <asm/pgtable.h>

static inline
unsigned long l4x_map_page_attr_to_l4(pte_t pte)
{
	switch (pte_val(pte) & L_PTE_MT_MASK) {
		case L_PTE_MT_UNCACHED:
			return L4_FPAGE_UNCACHEABLE;
		case L_PTE_MT_BUFFERABLE:
			return L4_FPAGE_BUFFERABLE;
		case L_PTE_MT_WRITEALLOC:
			return L4_FPAGE_CACHEABLE;
		case L_PTE_MT_DEV_WC:
		case L_PTE_MT_WRITETHROUGH:
			return L4_FPAGE_BUFFERABLE;
		default:
			return L4_FPAGE_CACHEABLE;
	};
}
