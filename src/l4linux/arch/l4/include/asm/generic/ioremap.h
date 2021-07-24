#ifndef __ASM_L4__GENERIC__IOREMAP_H__
#define __ASM_L4__GENERIC__IOREMAP_H__

unsigned long find_ioremap_entry(unsigned long phys_addr);
unsigned long find_ioremap_entry_phys(unsigned long virt_addr);
void l4x_ioremap_show(void);

#endif /* ! __ASM_L4__GENERIC__IOREMAP_H__ */
