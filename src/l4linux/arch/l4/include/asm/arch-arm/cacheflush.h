#ifndef __ASM_L4__ARCH_ARM__CACHEFLUSH_H__
#define __ASM_L4__ARCH_ARM__CACHEFLUSH_H__

#define __flush_icache_all __flush_icache_all_orig

/* Avoid copying the whole file, we just redefine some macros */
#include <asm-arm/cacheflush.h>

#undef flush_cache_vmap
#undef flush_cache_vunmap
#undef __flush_icache_all

#include <asm/generic/vmalloc.h>

#define flush_cache_vmap(start, end)            \
	do { l4x_vmalloc_map_vm_area(start, end); } while (0)
#define flush_cache_vunmap(start, end)          \
	do { l4x_vmalloc_unmap_vm_area(start, end); } while (0)

void l4x_flush_icache_all(void);
#define __flush_icache_all l4x_flush_icache_all

#endif /* __ASM_L4__ARCH_ARM__CACHEFLUSH_H__ */
