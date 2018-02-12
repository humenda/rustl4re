#ifndef __ASM_L4__GENERIC__VMALLOC_H__
#define __ASM_L4__GENERIC__VMALLOC_H__

void l4x_vmalloc_map_vm_area(unsigned long address, unsigned long end);
void l4x_vmalloc_unmap_vm_area(unsigned long address, unsigned long end);

#ifdef CONFIG_PM
void l4x_virtual_mem_register(unsigned long address, pte_t pte);
void l4x_virtual_mem_unregister(unsigned long address);
#else
static inline void l4x_virtual_mem_register(unsigned long address,
                                            pte_t pte)
{}
static inline void l4x_virtual_mem_unregister(unsigned long address)
{}
#endif

#endif /* ! __ASM_L4__GENERIC__VMALLOC_H__ */
