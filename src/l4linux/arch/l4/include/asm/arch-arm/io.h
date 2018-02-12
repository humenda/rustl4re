#ifndef __INCLUDE__ASM__ARCH_ARM__IO_H__
#define __INCLUDE__ASM__ARCH_ARM__IO_H__

#include <asm-arm/io.h>

#undef xlate_dev_mem_ptr
#undef xlate_dev_kmem_ptr

void *xlate_dev_mem_and_kmem_ptr_l4x(unsigned long x);

#define xlate_dev_mem_ptr(p) xlate_dev_mem_and_kmem_ptr_l4x(p)
#define xlate_dev_kmem_ptr(p) xlate_dev_mem_and_kmem_ptr_l4x((unsigned long)p)

#endif /* ! __INCLUDE__ASM__ARCH_ARM__IO_H__ */
