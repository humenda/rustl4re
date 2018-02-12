#ifndef __ASM_L4__L4X_ARM__DMA_H__
#define __ASM_L4__L4X_ARM__DMA_H__

#include <asm/memory.h>

#ifdef CONFIG_L4_DMAPOOL
int l4x_dmapool_mem_add(unsigned long virt, unsigned long phys, size_t size);
int l4x_dmapool_is_in_virt_dma_space(unsigned long va, size_t sz);
#else
static inline int l4x_dmapool_is_in_virt_dma_space(unsigned long va, size_t sz)
{
	return 0;
}
#endif

unsigned long l4x_arm_is_selfmapped_addr(unsigned long a);

#endif /* ! __ASM_L4__L4X_ARM__DMA_H__ */
