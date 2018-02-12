#ifndef __ASM_L4__ARCH_I386__CACHEFLUSH_H__
#define __ASM_L4__ARCH_I386__CACHEFLUSH_H__

/* Avoid copying the whole file, we just redefine some macros */
#include <asm-x86/cacheflush.h>
#undef flush_cache_vmap
#undef flush_cache_vunmap

#include <asm/generic/vmalloc.h>

static inline void flush_cache_vmap(unsigned long start, unsigned long end)
{
	l4x_vmalloc_map_vm_area(start, end);
}

static inline void flush_cache_vunmap(unsigned long start,
                                      unsigned long end)
{
	l4x_vmalloc_unmap_vm_area(start, end);
}

#endif /* __ASM_L4__ARCH_I386__CACHEFLUSH_H__ */
