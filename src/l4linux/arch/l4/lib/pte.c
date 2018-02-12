#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <asm/segment.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

#include <asm/api/config.h>

#include <asm/generic/memory.h>
#include <asm/generic/task.h>
#include <asm/generic/vmalloc.h>
#include <asm/generic/ioremap.h>
#include <asm/generic/log.h>

#include <asm/l4lxapi/memory.h>

#include <l4/sys/task.h>
#include <l4/sys/kdebug.h>
#include <l4/re/consts.h>

#ifdef CONFIG_ARM
#include <asm/l4x/dma.h>
#endif

enum {
	L4X_MM_CONTEXT_UNMAP_LOG_COUNT = 6,
};

struct unmap_log_entry_t {
	struct mm_struct *mm;
	unsigned long     addr;
	unsigned long     dbg1;
	unsigned char     rights;
	unsigned char     size;

};

struct unmap_log_t {
	unsigned cnt;
	struct unmap_log_entry_t log[L4X_MM_CONTEXT_UNMAP_LOG_COUNT];
};

static DEFINE_PER_CPU(struct unmap_log_t, unmap_log);

void l4x_pte_check_empty(struct mm_struct *mm)
{
	struct unmap_log_t *log;
	int i;

	WARN_ON(!irqs_disabled()); // otherwise we need to go non-preemtible

	log = this_cpu_ptr(&unmap_log);

	if (likely(this_cpu_read(unmap_log.cnt) == 0))
		return;

	for (i = 0; i < log->cnt; ++i) {
		if (mm != log->log[i].mm)
			continue;

		l4x_printf("L4x: exiting with non-flushed entry: %lx:%lx[sz=%d,r=%x,from=%lx,cpu=%d,num=%d]\n",
			   log->log[i].mm->context.task,
		           log->log[i].addr, log->log[i].size,
		           log->log[i].rights,
		           log->log[i].dbg1, raw_smp_processor_id(), i);
	}

	l4x_unmap_log_flush();
}

void l4x_unmap_log_flush(void)
{
	unsigned i;
	struct unmap_log_t *log;
	unsigned long flags;

	local_irq_save(flags);

	log = this_cpu_ptr(&unmap_log);

	for (i = 0; i < log->cnt; ++i) {
		l4_msgtag_t tag;
		struct mm_struct *mm = log->log[i].mm;

		if (unlikely(l4_is_invalid_cap(mm->context.task)))
			continue;

		tag = L4XV_FN(l4_msgtag_t,
		              l4_task_unmap(mm->context.task,
		                            l4_fpage(log->log[i].addr,
		                                     log->log[i].size,
		                                     log->log[i].rights),
		                            L4_FP_ALL_SPACES));
		if (unlikely(l4_error(tag))) {
			l4x_printf("l4_task_unmap error %ld: t=%lx\n",
			           l4_error(tag), mm->context.task);
			WARN_ON(1);
		} else if (0)
			l4x_printf("flushing(%d) %lx:%08lx[%d,%x]\n",
			           i, mm->context.task,
			           log->log[i].addr, log->log[i].size,
			           log->log[i].rights);
	}

	log->cnt = 0;
	local_irq_restore(flags);
}

static void empty_func(void *ignored)
{
}

static void unmap_log_add(struct mm_struct *mm,
                          unsigned long uaddr, unsigned size,
                          unsigned long rights,
                          unsigned long caller)
{
	int i, do_flush = 0;
	struct unmap_log_t *log;
	unsigned long flags;

	BUG_ON(!mm);

	local_irq_save(flags);

	log = this_cpu_ptr(&unmap_log);

	BUG_ON(log->cnt >= L4X_MM_CONTEXT_UNMAP_LOG_COUNT);

	i = log->cnt++;
	log->log[i].addr   = uaddr & (~0UL << size);
	log->log[i].mm     = mm;
	log->log[i].rights = rights;
	log->log[i].size   = size;
	log->log[i].dbg1   = caller;

	/* _simple_ merge with previous entries */
	while (i) {
		struct unmap_log_entry_t *prev = &log->log[i - 1];
		struct unmap_log_entry_t *cur  = &log->log[i];

		if (   prev->addr + (1 << prev->size) == cur->addr
		    && prev->size == cur->size
		    && (prev->addr & ((1 << (prev->size + 1)) - 1)) == 0
		    && prev->mm == cur->mm
		    && prev->rights == cur->rights) {
			prev->size += 1;
			log->cnt--;
			i--;
		} else
			break;
	}

	do_flush = log->cnt == L4X_MM_CONTEXT_UNMAP_LOG_COUNT;
	local_irq_restore(flags);

	if (do_flush) {
		on_each_cpu(empty_func, NULL, 1);
		l4x_unmap_log_flush();
	}
}

#ifdef CONFIG_X86
void l4x_pte_update(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	l4x_pte_clear(mm, addr, *ptep);
	l4x_unmap_log_flush();
}
#endif /* X86 */

static void l4x_flush_page(struct mm_struct *mm,
                           unsigned long address,
                           unsigned long vaddr,
                           int size,
                           unsigned long flush_rights, unsigned long caller)
{
	l4_msgtag_t tag;

	if (IS_ENABLED(CONFIG_ARM))
		return;

	if (mm && mm->context.l4x_unmap_mode == L4X_UNMAP_MODE_SKIP)
		return;

	if ((address & PAGE_MASK) == 0)
		address = PAGE0_PAGE_ADDRESS;

	if (likely(mm)) {
		unmap_log_add(mm, vaddr, size, flush_rights, caller);
		return;
	}

	/* do the real flush */
	if (mm && !l4_is_invalid_cap(mm->context.task)) {
		/* Direct flush in the child, use virtual address in the
		 * child address space */
		tag = L4XV_FN(l4_msgtag_t,
		              l4_task_unmap(mm->context.task,
		                           l4_fpage(vaddr & PAGE_MASK, size,
		                                    flush_rights),
		                           L4_FP_ALL_SPACES));
	} else {
		/* Flush all pages in all childs using the 'physical'
		 * address known in the Linux server */
		tag = L4XV_FN(l4_msgtag_t,
		              l4_task_unmap(L4RE_THIS_TASK_CAP,
			                    l4_fpage(address & PAGE_MASK, size,
		                                     flush_rights),
			                    L4_FP_OTHER_SPACES));
	}

	if (l4_error(tag))
		l4x_printf("l4_task_unmap error %ld\n", l4_error(tag));
}

unsigned long l4x_set_pte(struct mm_struct *mm,
                          unsigned long addr,
                          pte_t old, pte_t pteval)
{
	/*
	 * Check if any invalidation is necessary
	 *
	 * Invalidation (flush) necessary if:
	 *   old page was present
	 *       new page is not present OR
	 *       new page has another physical address OR
	 *       new page has another protection OR
	 *       new page has other access attributes
	 */

	/* old was present && new not -> flush */
	int flush_rights = L4_FPAGE_RWX;

	if (pte_present(pteval)) {
		/* new page is present,
		 * now we have to find out what has changed */
		if (((pte_val(old) ^ pte_val(pteval)) & L4X_PHYSICAL_PAGE_MASK)
		    || (pte_young(old) && !pte_young(pteval))) {
			/* physical page frame changed
			 * || access attribute changed -> flush */
			/* flush is the default */
		} else if ((pte_write(old) && !pte_write(pteval))
		           || (pte_dirty(old) && !pte_dirty(pteval))) {
			/* Protection changed from r/w to ro
			 * or page now clean -> remap */
			flush_rights = L4_FPAGE_W;
		} else {
			/* nothing changed, simply return */
			return pte_val(pteval);
		}
	}

	/* Ok, now actually flush or remap the page */
	l4x_flush_page(mm, pte_val(old) & L4X_PHYSICAL_PAGE_MASK,
	               addr, PAGE_SHIFT, flush_rights, _RET_IP_);
	return pte_val(pteval);
}

void l4x_pte_clear(struct mm_struct *mm, unsigned long addr, pte_t pteval)
{
	/* Invalidate page */
	l4x_flush_page(mm, pte_val(pteval) & L4X_PHYSICAL_PAGE_MASK,
	               addr, PAGE_SHIFT, L4_FPAGE_RWX, _RET_IP_);
}

#ifdef CONFIG_X86
static inline
void _l4x_pmd_clear(struct mm_struct *mm, unsigned long addr, pmd_t pmdval)
{
	/* Invalidate page */
	l4x_flush_page(mm, pmd_val(pmdval) & PMD_PAGE_MASK & PHYSICAL_PAGE_MASK,
	               addr, PMD_SHIFT, L4_FPAGE_RWX, _RET_IP_);
}

static inline
void _pmd_update(struct mm_struct *mm, unsigned long addr, pmd_t *pmdp)
{
	_l4x_pmd_clear(mm, addr, *pmdp);
	l4x_unmap_log_flush();
}

void l4x_pmd_update(struct mm_struct *mm, unsigned long addr, pmd_t *pmdp)
{
	_pmd_update(mm, addr, pmdp);
}

void l4x_pmd_update_defer(struct mm_struct *mm, unsigned long addr, pmd_t *pmdp)
{
	_pmd_update(mm, addr, pmdp);
}
#endif /* X86 */

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
unsigned long l4x_set_pmd(struct mm_struct *mm,
                          unsigned long addr,
                          pmd_t old, pmd_t pmdval)
{
	/*
	 * Check if any invalidation is necessary
	 *
	 * Invalidation (flush) necessary if:
	 *   old page was present
	 *       new page is not present OR
	 *       new page has another physical address OR
	 *       new page has another protection OR
	 *       new page has other access attributes
	 */

	/* old was present && new not -> flush */
	int flush_rights = L4_FPAGE_RWX;
	BUG_ON(!pmd_large(old));

	if (pmd_present(pmdval)) {
		/* new page is present,
		 * now we have to find out what has changed */
		if (((pmd_val(old) ^ pmd_val(pmdval)) & PMD_PAGE_MASK & PHYSICAL_PAGE_MASK)
		    || (pmd_young(old) && !pmd_young(pmdval))) {
			/* physical page frame changed
			 * || access attribute changed -> flush */
			/* flush is the default */
		} else if ((pmd_write(old) && !pmd_write(pmdval))
		           || (pmd_flags(old) & ~pmd_flags(pmdval)
		               & (_PAGE_DIRTY | _PAGE_SOFT_DIRTY))) {
			/* Protection changed from r/w to ro
			 * or page now clean -> remap */
			flush_rights = L4_FPAGE_W;
		} else {
			/* nothing changed, simply return */
			return pmd_val(pmdval);
		}
	}

	/* Ok, now actually flush or remap the page */
	l4x_flush_page(mm, pmd_val(old) & PMD_PAGE_MASK & PHYSICAL_PAGE_MASK,
	               addr, PMD_SHIFT, flush_rights, _RET_IP_);
	return pmd_val(pmdval);
}

void l4x_pmd_clear(struct mm_struct *mm, unsigned long addr, pmd_t pmdval)
{
	_l4x_pmd_clear(mm, addr, pmdval);
}
#endif

/* (Un)Mapping function for vmalloc'ed memory */

void l4x_vmalloc_map_vm_area(unsigned long address, unsigned long end)
{
	unsigned page_shift = 0;

	if (address & ~PAGE_MASK)
		enter_kdebug("map_vm_area: Unaligned address!");

	if (!(   (VMALLOC_START <= address && end <= VMALLOC_END)
	      || (MODULES_VADDR <= address && end <= MODULES_END))) {
		pr_err("%s: %lx-%lx outside areas: %lx-%lx, %lx-%lx\n",
		       __func__, address, end,
		       VMALLOC_START, VMALLOC_END, MODULES_VADDR, MODULES_END);
		pr_err("%s: %p\n", __func__, __builtin_return_address(0));
		enter_kdebug("KK");
		return;
	}

	for (; address < end; address += PAGE_SIZE) {
		pte_t *ptep;

#ifdef CONFIG_ARM
		unsigned long o;
		if ((o = l4x_arm_is_selfmapped_addr(address))) {
			address += o - PAGE_SIZE;
			continue;
		}
#endif

		ptep = lookup_pte(&init_mm, address, &page_shift);

		if (!ptep || !pte_present(*ptep)) {
			if (0)
				printk("%s: No (valid) PTE for %08lx?!"
			               " (ptep: %p, pte: %08lx\n",
			               __func__, address,
			               ptep, (unsigned long)pte_val(*ptep));
			continue;
		}
		BUG_ON(page_shift != PAGE_SHIFT);
		l4x_virtual_mem_register(address, *ptep);
		l4lx_memory_map_virtual_page(address, *ptep);
	}
}


void l4x_vmalloc_unmap_vm_area(unsigned long address, unsigned long end)
{
	if (address & ~PAGE_MASK)
		enter_kdebug("unmap_vm_area: Unaligned address!");

	for (; address < end; address += PAGE_SIZE) {

#ifdef CONFIG_ARM
		unsigned long o;
		if ((o = l4x_arm_is_selfmapped_addr(address))) {
			address += o - PAGE_SIZE;
			continue;
		}
#endif

		/* check whether we are really flushing a vm page */
		if (address < (unsigned long)high_memory
#if defined(CONFIG_X86_64) || defined(CONFIG_ARM)
		    && !(address >= MODULES_VADDR && address < MODULES_END)
#endif
		    ) {
			printk("flushing wrong page, addr: %lx\n", address);
			enter_kdebug("l4x_vmalloc_unmap_vm_area");
			continue;
		}
		l4x_virtual_mem_unregister(address);
		l4lx_memory_unmap_virtual_page(address);
	}
}
