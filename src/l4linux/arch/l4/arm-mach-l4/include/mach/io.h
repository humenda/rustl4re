/*
 * linux/include/asm-l4/arch-arm/arch/io.h
 */
#ifndef __ASM_L4__ARCH_ARM__ARCH__IO_H__
#define __ASM_L4__ARCH_ARM__ARCH__IO_H__

#define __ARCH_HAS_NO_PAGE_ZERO_MAPPED	1

#define IO_SPACE_LIMIT 0xd0000000

#define __io(a)			__typesafe_io(a)
#define __mem_pci(a)		(a)

#define IO_ADDRESS(n) ((void __iomem *)n)

#endif /* ! __ASM_L4__ARCH_ARM__ARCH__IO_H__ */
