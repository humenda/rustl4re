/*
 * Some processor specific functions.
 *
 * Maybe we should define an L4 CPU type somewhere?
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/highmem.h>
#include <linux/dma-mapping.h>

#include <asm/cacheflush.h>
#include <asm/elf.h>
#include <asm/page.h>
#include <asm/procinfo.h>
#include <asm/tlbflush.h>
#include <asm/smp_plat.h>

#include <l4/sys/kdebug.h>
#include <l4/sys/cache.h>

#include <asm/generic/memory.h>
#include <asm/generic/setup.h>
#include <asm/generic/tamed.h>
#ifndef CONFIG_SMP
#include <asm/generic/tlb.h>
#endif

void __glue(CPU_NAME, _dcache_clean_area)(void *addr, int sz)
{ l4_cache_clean_data((unsigned long)addr, (unsigned long)addr + sz - 1); }

void __glue(CPU_NAME, _switch_mm)(phys_addr_t pgd_phys,
                                  struct mm_struct *mm)
{}

extern unsigned long l4x_set_pte(struct mm_struct *mm, unsigned long addr, pte_t pteptr, pte_t pteval);
extern void          l4x_pte_clear(struct mm_struct *mm, unsigned long addr, pte_t ptep);

void __glue(CPU_NAME, _set_pte_ext)(pte_t *pteptr, pte_t pteval,
                                   unsigned int ext)
{
	if (pte_present(*pteptr)) {
		if (pteval == __pte(0))
			l4x_pte_clear(NULL, 0, *pteptr);
		else
			pte_val(pteval) = l4x_set_pte(NULL, 0, *pteptr, pteval);
	}
	*pteptr = pteval;
}

/*
 * cpu_do_idle()
 * Cause the processor to idle
 */
int __glue(CPU_NAME, _do_idle)(void)
{
#ifdef CONFIG_L4_VCPU
	l4x_global_halt();
#endif
	return 0;
}

void __glue(CPU_NAME, _proc_init)(void)
{}

/*
 * cpu_proc_fin()
 *
 * Prepare the CPU for reset:
 *  - Disable interrupts
 *  - Clean and turn off caches.
 */
void __glue(CPU_NAME, _proc_fin)(void) { }

void  __attribute__((noreturn))
__glue(CPU_NAME, _reset)(unsigned long addr, bool hvc)
{
	l4x_exit_l4linux_msg("%s called.\n", __func__);
	while (1)
		;
}

static inline void __copy_user_highpage(struct page *to, struct page *from,
                                        unsigned long vaddr, struct vm_area_struct *vma)
{
	void *kto, *kfrom;

        kfrom = kmap_atomic(from);
        kto = kmap_atomic(to);
        copy_page(kto, kfrom);
        kunmap_atomic(kto);
        kunmap_atomic(kfrom);
}

void __glue(_USER, _copy_user_highpage)(struct page *to, struct page *from,
                                        unsigned long vaddr, struct vm_area_struct *vma)
{
	__copy_user_highpage(to, from, vaddr, vma);
}

static inline void __clear_user_highpage(struct page *page, unsigned long vaddr)
{
	void *kaddr = kmap_atomic(page);
	clear_page(kaddr);
	kunmap_atomic(kaddr);
}

void __glue(_USER, _clear_user_highpage)(struct page *page, unsigned long vaddr)
{
	__clear_user_highpage(page, vaddr);
}

void __glue(_TLB, _flush_user_tlb_range)(unsigned long start, unsigned long end,
                                          struct vm_area_struct *vma)
{
#ifdef CONFIG_SMP
	BUG();
#else
	l4x_unmap_page_range(vma->vm_mm, start, end);
#endif
}

void __glue(_CACHE, _flush_user_cache_range)(unsigned long start, unsigned long end,
                                            unsigned int flags)
{
	struct mm_struct *mm;
	unsigned page_shift = 0;

	if (current->mm)
		mm = current->mm;
	else if (current->active_mm)
		mm = current->active_mm;
	else {
		printk("active_mm: No mm... %lx-%lx\n", start, end);
		return;
	}

	for (start &= PAGE_MASK; start < end; start += PAGE_SIZE) {
		pte_t *ptep = lookup_pte(mm, start, &page_shift);
		if (ptep && pte_present(*ptep)) {
			unsigned long k = pte_pfn(*ptep) << PAGE_SHIFT;
			unsigned long e = k + PAGE_SIZE;
			l4_cache_flush_data(k, e);
		}
	}
}

void __glue(_CACHE, _flush_user_cache_all)(void)
{
}

void __glue(_TLB, _flush_kern_tlb_range)(unsigned long start, unsigned long end)
{
}

void __glue(_CACHE, _flush_kern_cache_all)(void)
{
	l4_cache_dma_coherent_full();
}

void __glue(_CACHE, _flush_kern_cache_louis)(void)
{
	l4_cache_dma_coherent_full();
}

void __glue(_CACHE, _coherent_kern_range)(unsigned long start, unsigned long end)
{
	l4_cache_coherent(start, end);
}

int __glue(_CACHE, _coherent_user_range)(unsigned long start, unsigned long end)
{
	struct mm_struct *mm;
	unsigned page_shift = 0;

	if (current->mm)
		mm = current->mm;
	else if (current->active_mm)
		mm = current->active_mm;
	else {
		printk("active_mm: No mm... %lx-%lx\n", start, end);
		return -EINVAL;
	}

	for (start &= PAGE_MASK; start < end; start += PAGE_SIZE) {
		pte_t *ptep = lookup_pte(mm, start, &page_shift);
		if (ptep && pte_present(*ptep)) {
			unsigned long k = pte_pfn(*ptep) << PAGE_SHIFT;
			unsigned long e = k + PAGE_SIZE;
			l4_cache_coherent(k, e);
		}
	}

	return 0;
}

void __glue(_CACHE, _flush_kern_dcache_area)(void *x, size_t size)
{
	l4_cache_flush_data((unsigned long)x,
	                    (unsigned long)x + size - 1);
}

void l4x_flush_icache_all(void)
{
}

void __glue(_CACHE, _flush_icache_all)(void)
{
}

static void __data_abort(unsigned long pc)
{
	printk("%s called.\n", __func__);
}

void __glue(_CACHE, _dma_flush_range)(const void *start, const void *stop)
{
	l4_cache_flush_data((unsigned long)start, (unsigned long)stop);
}

void __glue(_CACHE, _dma_map_area)(const void *start, size_t sz, int direction)
{
	if (direction == DMA_FROM_DEVICE)
		l4_cache_inv_data((unsigned long)start,
		                  (unsigned long)start + sz);
	else
		l4_cache_clean_data((unsigned long)start,
		                    (unsigned long)start + sz);
}

void __glue(_CACHE, _dma_unmap_area)(const void *start, size_t sz, int direction)
{
	if (direction != DMA_TO_DEVICE)
		l4_cache_inv_data((unsigned long)start,
		                  (unsigned long)start + sz);
}


static struct processor l4_proc_fns = {
	._data_abort         = __data_abort,
	._proc_init          = __glue(CPU_NAME, _proc_init),
	._proc_fin           = __glue(CPU_NAME, _proc_fin),
	.reset               = __glue(CPU_NAME, _reset),
	._do_idle            = __glue(CPU_NAME, _do_idle),
	.dcache_clean_area   = __glue(CPU_NAME, _dcache_clean_area),
	.switch_mm           = __glue(CPU_NAME, _switch_mm),
	.set_pte_ext         = __glue(CPU_NAME, _set_pte_ext),
	//.suspend_size = ...,
	//.do_suspend = ...,
	//.do_resume = ...,
};

static struct cpu_tlb_fns l4_tlb_fns = {
	.flush_user_range    = __glue(_TLB, _flush_user_tlb_range),
	.flush_kern_range    = __glue(_TLB, _flush_kern_tlb_range),
	.tlb_flags           = 0,
};

static struct cpu_user_fns l4_cpu_user_fns = {
	.cpu_clear_user_highpage = __glue(_USER, _clear_user_highpage),
	.cpu_copy_user_highpage  = __glue(_USER, _copy_user_highpage),
};

static struct cpu_cache_fns l4_cpu_cache_fns = {
	.flush_icache_all       = __glue(_CACHE, _flush_icache_all),
	.flush_kern_all         = __glue(_CACHE, _flush_kern_cache_all),
	.flush_kern_louis       = __glue(_CACHE, _flush_kern_cache_louis),
	.flush_user_all         = __glue(_CACHE, _flush_user_cache_all),
	.flush_user_range       = __glue(_CACHE, _flush_user_cache_range),
	.coherent_kern_range    = __glue(_CACHE, _coherent_kern_range),
	.coherent_user_range    = __glue(_CACHE, _coherent_user_range),
	.flush_kern_dcache_area = __glue(_CACHE, _flush_kern_dcache_area),
	.dma_map_area           = __glue(_CACHE, _dma_map_area),
	.dma_unmap_area         = __glue(_CACHE, _dma_unmap_area),
	.dma_flush_range	= __glue(_CACHE, _dma_flush_range),
};

static struct proc_info_list l4_proc_info __attribute__((__section__(".proc.info.init"))) = {
	.cpu_val         = 0,
	.cpu_mask        = 0,
	.__cpu_mm_mmu_flags = PMD_SECT_WBWA,
	.__cpu_io_mmu_flags = 0,
	.__cpu_flush     = 0,
#ifdef CONFIG_CPU_V7
	.arch_name       = "armv7",
	.elf_name        = "v7",
#elif !defined(CONFIG_CPU_V6K)
	.arch_name       = "armv5",
	.elf_name        = "v5",
#else
	.arch_name       = "armv6",
	.elf_name        = "v6",
#endif
	.elf_hwcap       = HWCAP_SWP | HWCAP_THUMB | HWCAP_HALF | HWCAP_26BIT | HWCAP_FAST_MULT,
	.cpu_name        = "Fiasco",
	.proc            = &l4_proc_fns,
	.tlb             = &l4_tlb_fns,
	.user            = &l4_cpu_user_fns,
	.cache           = &l4_cpu_cache_fns,
};

/*
 * This is the only processor info for now, so keep lookup_processor_type
 * simple.
 */
struct proc_info_list *lookup_processor_type(void);
struct proc_info_list *lookup_processor_type(void)
{
	BUILD_BUG_ON(sizeof(l4_cpu_cache_fns) != 44);
	BUILD_BUG_ON(sizeof(l4_tlb_fns) != 12);
	BUILD_BUG_ON(sizeof(l4_cpu_user_fns) != 8);

	if (is_smp())
		l4_proc_info.__cpu_mm_mmu_flags |= PMD_SECT_S;

	return &l4_proc_info;
}
