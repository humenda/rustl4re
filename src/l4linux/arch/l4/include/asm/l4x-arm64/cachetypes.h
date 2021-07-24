#pragma once

#include <asm/pgtable.h>

static inline
unsigned long l4x_map_page_attr_to_l4(pte_t pte)
{
	switch (pte_val(pte) & PTE_ATTRINDX_MASK) {
		case PTE_ATTRINDX(MT_DEVICE_nGnRnE):
		case PTE_ATTRINDX(MT_DEVICE_nGnRE):
		case PTE_ATTRINDX(MT_DEVICE_GRE):
			return L4_FPAGE_UNCACHEABLE;
		case PTE_ATTRINDX(MT_NORMAL_NC):
		case PTE_ATTRINDX(MT_NORMAL_WT):
			return L4_FPAGE_BUFFERABLE;
		default:
			return L4_FPAGE_CACHEABLE;
	};
}
