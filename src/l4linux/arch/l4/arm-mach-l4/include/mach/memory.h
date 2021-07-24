#ifndef __ASM_L4__ARCH_ARM__ARCH__MEMORY_H__
#define __ASM_L4__ARCH_ARM__ARCH__MEMORY_H__

#ifdef CONFIG_DISCONTIGMEM
/*
 * The nodes are matched with the physical SDRAM banks as follows:
 *
 * 	node 0:  0xa0000000-0xa3ffffff	-->  0xc0000000-0xc3ffffff
 * 	node 1:  0xa4000000-0xa7ffffff	-->  0xc4000000-0xc7ffffff
 * 	node 2:  0xa8000000-0xabffffff	-->  0xc8000000-0xcbffffff
 * 	node 3:  0xac000000-0xafffffff	-->  0xcc000000-0xcfffffff
 */

#define NR_NODES	4

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(addr) (((unsigned long)(addr) - PAGE_OFFSET) >> 26)

/*
 * Given a page frame number, convert it to a node id.
 */
#define PFN_TO_NID(pfn)		(((pfn) - PHYS_PFN_OFFSET) >> (26 - PAGE_SHIFT))

/*
 * Given a kaddr, ADDR_TO_MAPBASE finds the owning node of the memory
 * and returns the mem_map of that node.
 */
#define ADDR_TO_MAPBASE(kaddr)	NODE_MEM_MAP(KVADDR_TO_NID(kaddr))

/*
 * Given a page frame number, find the owning node of the memory
 * and returns the mem_map of that node.
 */
#define PFN_TO_MAPBASE(pfn)	NODE_MEM_MAP(PFN_TO_NID(pfn))

/*
 * Given a kaddr, LOCAL_MEM_MAP finds the owning node of the memory
 * and returns the index corresponding to the appropriate page in the
 * node's mem_map.
 */
#define LOCAL_MAP_NR(addr) \
	(((unsigned long)(addr) & 0x03ffffff) >> PAGE_SHIFT)

#endif

#define __arch_pfn_to_dma(dev, pfn) \
	((dma_addr_t)l4x_virt_to_phys((void *)((phys_addr_t)(pfn) << PAGE_SHIFT)))

#define __arch_dma_to_pfn(dev, addr) \
	(((unsigned long)l4x_phys_to_virt(addr)) >> PAGE_SHIFT)

#define __arch_dma_to_virt(dev, addr) \
	l4x_phys_to_virt(addr)

#define __arch_virt_to_dma(dev, addr) \
	((dma_addr_t)l4x_virt_to_phys(addr))


#endif /* ! __ASM_L4__ARCH_ARM__ARCH__MEMORY_H__ */
