#ifndef __INCLUDE__ASM_L4__GENERIC__TLB_H__
#define __INCLUDE__ASM_L4__GENERIC__TLB_H__

#include <l4/sys/task.h>
#include <asm/l4lxapi/task.h>

static inline
void l4x_del_task(struct mm_struct *mm)
{
	l4_cap_idx_t taskcap = READ_ONCE(mm->context.task);
#ifndef CONFIG_L4_VCPU
	l4_msgtag_t tag;
#endif

	if (l4_is_invalid_cap(taskcap))
		return;

#ifdef CONFIG_L4_VCPU
	if (taskcap == cmpxchg(&mm->context.task, taskcap, L4_INVALID_CAP)) {
		l4lx_task_delete_task(taskcap);
		l4lx_task_number_free(taskcap);
	}
#else
	tag = L4XV_FN(l4_msgtag_t,
	              l4_task_unmap(taskcap,
	                            l4_fpage(0, L4_WHOLE_ADDRESS_SPACE,
	                                     L4_FPAGE_RWX),
	                            L4_FP_ALL_SPACES));

	if (l4_error(tag))
		printk("l4_task_unmap error %ld\n", l4_error(tag));
#endif
}

static inline
void l4x_unmap_page(struct mm_struct *mm, unsigned long uaddr)
{
	l4_msgtag_t tag;
	l4_cap_idx_t taskcap = READ_ONCE(mm->context.task);

	if (l4_is_invalid_cap(taskcap))
		return;

	tag = L4XV_FN(l4_msgtag_t,
	              l4_task_unmap(taskcap,
	                            l4_fpage(uaddr & PAGE_MASK, 12,
	                                     L4_FPAGE_RWX),
	                            L4_FP_ALL_SPACES));

	if (l4_error(tag))
		printk("l4_task_unmap error %ld\n", l4_error(tag));
}

static inline
void l4x_unmap_page_range(struct mm_struct *mm,
                          unsigned long start, unsigned long end)
{
	for (; start < end; start += PAGE_SIZE)
		l4x_unmap_page(mm, start);

}

#endif /* ! __INCLUDE__ASM_L4__GENERIC__TLB_H__ */
