#include <asm/page.h>
#include <asm/generic/memory.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <linux/errno.h>
#include <linux/module.h>

#include <asm/api/macros.h>
#include <l4/sys/kdebug.h>

enum {
	Debug_log_efault = 0,
	Debug_kdebug_efault = 0,
	Debug_memcpy_kernel = 0,
	Debug_memcpy_fromfs = 0,
	Debug_memcpy_tofs = 0,
};

// from user_copy_32.c
#ifdef CONFIG_X86_INTEL_USERCOPY
/*
 * Alignment at which movsl is preferred for bulk memory copies.
 */
struct movsl_mask movsl_mask __read_mostly;
#endif

static void do_log_efault(const char *str, const void *user_addr,
                          const void *kernel_addr, unsigned long size)
{
	unsigned int page_shift;
	pte_t *ptep = lookup_pte(current->mm,
				 (unsigned long)user_addr, &page_shift);

	printk("%s returning efault, \n"
	       "  user_addr: %p, kernel_addr: %p, size: %08lx\n"
#ifndef CONFIG_L4_VCPU
	       "  task: %s (%p) " PRINTF_L4TASK_FORM
#else
	       "  task: %s (%p) "
#endif
	       ", pdir: %p, ptep: %p, pte: %llx\n",
	       str, user_addr, kernel_addr, size,
	       current->comm, current,
#ifndef CONFIG_L4_VCPU
	       PRINTF_L4TASK_ARG(current->thread.l4x.user_thread_id),
#endif
	       current->mm->pgd, ptep,
	       ptep ? (unsigned long long)pte_val(*ptep) : 0ULL);
	if (Debug_kdebug_efault)
		enter_kdebug("log_efault");
}

static inline void log_efault(const char *str, const void *user_addr,
                              const void *kernel_addr, unsigned long size)
{
	if (Debug_log_efault)
		do_log_efault(str, user_addr, kernel_addr, size);
}

static inline int __copy_to_user_page(void *to, const void *from,
				      unsigned long n)
{
	unsigned long page, offset, flags;

	if (Debug_memcpy_tofs)
		printk("__copy_to_user_page to: %p, from: %p, len: %08lx\n",
		       to, from, n);

	if ((page = parse_ptabs_write((unsigned long)to, &offset, &flags)) != -EFAULT) {
		if (Debug_memcpy_tofs)
			printk("    __copy_to_user_page writing to: %08lx\n",
			       (page + offset));

		memcpy((void *)(page + offset), from, n);
		local_irq_restore(flags);
		return 0;
	}
	log_efault("__copy_to_user_page", to, from, n);
	return -EFAULT;
}

unsigned long __must_check
l4x_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	unsigned long copy_size = (unsigned long)to & ~PAGE_MASK;

	if (Debug_memcpy_tofs)
		printk("l4x_copy_to_user called from: %08lx to: %p, "
		       "from: %p, len: %08lx\n",
		       *((unsigned long *)&to - 1), to, from, n);

	/* kernel access */
	if (segment_eq(get_fs(), KERNEL_DS)) {
		if (L4X_CHECK_IN_KERNEL_ACCESS && l4x_check_kern_region(to, n, 1))
			return -EFAULT;
		memcpy(to, from, n);
		return 0;
	}

	if (copy_size) {
		copy_size = min(PAGE_SIZE - copy_size, n);
		if (__copy_to_user_page(to, from, copy_size) == -EFAULT)
			return n;
		n -= copy_size;
	}

	while (n) {
		from +=copy_size;
		to += copy_size;
		copy_size = min(PAGE_SIZE, n);
		if (__copy_to_user_page(to, from, copy_size) == -EFAULT)
			return n;
		n -= copy_size;
	}
	return 0;
}
EXPORT_SYMBOL(l4x_copy_to_user);

static inline int __copy_from_user_page(void *to, const void *from,
					unsigned long n)
{
	unsigned long page, offset, flags;

	if (Debug_memcpy_fromfs)
		printk("%s to: %p, from: %p, len: %08lx\n", __func__,
		       to, from, n);

	if ((page = parse_ptabs_read((unsigned long)from, &offset, &flags)) != -EFAULT) {
		if (Debug_memcpy_fromfs)
			printk("  %s reading from: %08lx\n",
			       __func__, (page + offset));

		memcpy(to, (void *)(page + offset), n);
		local_irq_restore(flags);
		return 0;
	}
	log_efault(__func__, from, to, n);
	return -EFAULT;
}

unsigned long
l4x_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long copy_size = (unsigned long)from & ~PAGE_MASK;

	if (segment_eq(get_fs(), KERNEL_DS)) {
		if (L4X_CHECK_IN_KERNEL_ACCESS && l4x_check_kern_region((void *)from, n, 0))
			return -EFAULT;
		memcpy(to, from, n);
		return 0;
	}

	if (Debug_memcpy_fromfs)
		printk("l4x_copy_from_user called from: %08lx "
		       "to: %p, from: %p, len: %08lx\n",
		       *((unsigned long *)&to - 1), to, from, n);

	if (copy_size) {
		copy_size = min(PAGE_SIZE - copy_size, n);
		if (__copy_from_user_page(to, from, copy_size) == -EFAULT) {
			memset(to, 0, n);
			return n;
		}
		n -= copy_size;
	}
	while (n) {
		from +=copy_size;
		to += copy_size;
		copy_size = min(PAGE_SIZE, n);
		if (__copy_from_user_page(to, from, copy_size) == -EFAULT) {
			memset(to, 0, n);
			return n;
		}
		n -= copy_size;
	}
	return 0;
}
EXPORT_SYMBOL(l4x_copy_from_user);

static inline int __clear_user_page(void * address, unsigned long n)
{
	unsigned long page, offset, flags;

	if (Debug_memcpy_tofs)
		printk("%s: %p, len: %08lx\n", __func__, address, n);

	page = parse_ptabs_write((unsigned long)address, &offset, &flags);
	if (page != -EFAULT) {
		if (Debug_memcpy_tofs)
			printk("    writing to: %08lx\n", (page + offset));

		memset((void *)(page + offset), 0, n);
		local_irq_restore(flags);
		return 0;
	}
	log_efault(__func__, address, 0, n);
	return -EFAULT;
}

unsigned long l4x_clear_user(void __user *address, unsigned long n)
{
	unsigned long clear_size = (unsigned long)address & ~PAGE_MASK;

	if (Debug_memcpy_tofs)
		printk("%s called from: %08lx to: %p, len: %08lx\n",
		       __func__, *((unsigned long *)&address - 1), address, n);

	if (segment_eq(get_fs(), KERNEL_DS)) {
		if (L4X_CHECK_IN_KERNEL_ACCESS && l4x_check_kern_region(address, n, 1))
			return -EFAULT;
		memset(address, 0, n);
		return 0;
	}

	if (clear_size) {
		clear_size = min(PAGE_SIZE - clear_size, n);
		if (__clear_user_page(address, clear_size) == -EFAULT)
			return n;
		n -= clear_size;
	}
	while (n) {
		address += clear_size;
		clear_size = min(PAGE_SIZE, n);
		if (__clear_user_page(address, clear_size) == -EFAULT)
			return n;
		n -= clear_size;
	}
	return 0;
}
EXPORT_SYMBOL(l4x_clear_user);

#ifdef CONFIG_X86
unsigned long
__clear_user(void __user *to, unsigned long n)
{
	return l4x_clear_user(to, n);
}
EXPORT_SYMBOL(__clear_user);
#endif

/*
 * Copy a null terminated string from userspace.
 */

#ifdef CONFIG_X86_32
#define __do_strncpy_from_user_page(dst,src,count)			   \
do {									   \
	int __d0, __d1, __d2;						   \
	__asm__ __volatile__(						   \
		"	testl %0,%0\n"					   \
		"	jz 1f\n"					   \
		"0:	lodsb\n"					   \
		"	stosb\n"					   \
		"	testb %%al,%%al\n"				   \
		"	jz 1f\n"					   \
		"	decl %0\n"					   \
		"	jnz 0b\n"					   \
		"1:\n"							   \
		: "=c"(count), "=&a" (__d0), "=&S" (__d1),		   \
		  "=&D" (__d2)						   \
		: "0"(count), "2"(src), "3"(dst)			   \
		: "memory");						   \
} while (0)
#else
#define __do_strncpy_from_user_page(dst, src, count)                    \
	do {								\
		char *_src = src;					\
		while (count) {						\
			*dst = *_src;					\
			if (!*_src)					\
				break;					\
			dst++;						\
			_src++;						\
			count--;					\
		}							\
	} while (0)
#endif

static inline int __strncpy_from_user_page(char * to, const char * from,
					   unsigned long n)
{
	unsigned long page, offset, flags;

	if (Debug_memcpy_fromfs)
		printk("%s: to: %p, from: %p, len: %08lx\n",
		       __func__, to, from, n);

	page = parse_ptabs_read((unsigned long)from, &offset, &flags);
	if (page != -EFAULT) {
		if (Debug_memcpy_fromfs)
			printk("    %s reading from: %08lx len: 0x%lx\n",
			       __func__, (page + offset), n);

		/* after finishing the copy operation count is either
		 * - zero: max number of bytes copied or
		 * - non zero: end of string reached, n containing the
		 *             number of remaining bytes
		 */
		__do_strncpy_from_user_page(to, (char *)(page + offset), n);
		local_irq_restore(flags);
		return n;
	}
	log_efault(__func__, from, to, n);
	return -EFAULT;
}

/* strncpy returns the number of bytes copied. We calculate the number
 * simply by substracting the number of bytes remaining from the
 * maximal length. The number of bytes remaining is (n + res) with n
 * beeing the number of bytes to copy from the next pages and res the
 * number of remaining bytes after reaching the '\0' */

long l4x_strncpy_from_user(char *dst, const char *src, long count)
{
	unsigned long copy_size = (unsigned long)src & ~PAGE_MASK;
	long res;
	unsigned long n = count;

	if (Debug_memcpy_fromfs)
		printk("l4x_strncpy_from_user called from: %08lx "
		       "to: %p, from: %p, len: 0x%lx (copy_size: 0x%lx)\n",
		       *((unsigned long *)&dst - 1), dst, src, n, copy_size);

	if (segment_eq(get_fs(), KERNEL_DS)) {
		/* strncpy the data but deliver back the bytes copied */
		long c = 0;
		if (L4X_CHECK_IN_KERNEL_ACCESS && l4x_check_kern_region(dst, count, 1))
			return -EFAULT;
		while (c++ < count && (*dst++ = *src++) != '\0')
			/* nothing */;
		return c;
	}

	if (copy_size) {
		copy_size = min(PAGE_SIZE - copy_size, n);
		res = __strncpy_from_user_page(dst, src, copy_size);
		n -= copy_size;
		if (res == -EFAULT) {
			return -EFAULT;
		}
		else if (res)
			return count - (n + res);
	}
	while (n) {
		src += copy_size;
		dst += copy_size;
		copy_size = min(PAGE_SIZE, n);
		n -= copy_size;
		res = __strncpy_from_user_page(dst, src, copy_size);
		if (res == -EFAULT) {
			return -EFAULT;
		}
		else if (res)
			return count - (n + res);
	}
	return count;
}
EXPORT_SYMBOL(l4x_strncpy_from_user);

static inline int __strnlen_from_user_page(const char *from,
					   unsigned long n, unsigned long *len)
{
	unsigned long page, offset, flags;

	if (Debug_memcpy_fromfs)
		printk("%s from: %p, len: %08lx\n", __func__, from, n);

	page = parse_ptabs_read((unsigned long)from, &offset, &flags);
	if (page != -EFAULT) {
		int end;
		if (Debug_memcpy_fromfs)
			printk("    %s reading from: %08lx\n",
			       __func__, (page + offset));

#ifdef CONFIG_X86
		{
			int res;
			__asm__ __volatile__(
				"0:	repne; scasb\n"
				"	sete %b1\n"
				:"=c" (res),  "=a" (end)
				:"0" (n), "1" (0), "D" ((page + offset))
				);
			/* after finishing the search operation 'end' is either
			 * - zero: max number of bytes searched
			 * - non zero: end of string reached, res containing
			 *      the number of remaining bytes
			 */
			*len += n - res;
		}
#elif defined(CONFIG_ARM)
		{
			unsigned long i, p = page + offset;
			end = 0;
			for (i = 0; i < n; i++) {
				if (*(unsigned char *)(p + i) == 0) {
					end = 1;
					i++; /* Include the zero */
					break;
				}
			}
			*len += i;
		}
#else
#error Add your architecture here
#endif
		local_irq_restore(flags);
		return end;
	}
	log_efault(__func__, from, 0, n);
	return -EFAULT;
}

/* strnlen returns the number of bytes in a string. We calculate the number
 * simply by substracting the number of bytes remaining from the
 * maximal length. The number of bytes remaining is (n + res) with n
 * being the number of bytes to copy from the next pages and res the
 * number of remaining bytes after reaching the '\0' */
long l4x_strnlen_user(const char *src, long n)
{
	unsigned long search_size = PAGE_SIZE - ((unsigned long)src & ~PAGE_MASK);
	int res;
	unsigned long len = 0;

	if (Debug_memcpy_fromfs)
		printk("l4x_strnlen_user called from: %08lx, from: %p, %ld\n",
		       *((unsigned long *)&src - 1), src, n);

	if (segment_eq(get_fs(), KERNEL_DS)) {
		if (L4X_CHECK_IN_KERNEL_ACCESS && l4x_check_kern_region((void *)src, n, 0))
			return -EFAULT;
		len = strnlen(src, n);
		if (Debug_memcpy_kernel)
			printk("kernel l4x_strnlen_user %p, %ld = %ld\n",
			       src, n, len);
		return len + 1;
	}

	if (!search_size)
		search_size = PAGE_SIZE;
	if (search_size > n)
		search_size = n;

	while (n > 0) {
		res = __strnlen_from_user_page(src, search_size, &len);
		if (res == -EFAULT)
			return 0; /* EFAULT */
		else if (res)
			return len;

		src += search_size;
		n   -= search_size;
		search_size = PAGE_SIZE;
	}

	return 0; /* EFAULT */
}
EXPORT_SYMBOL(l4x_strnlen_user);

long l4x_strlen_user(const char *src)
{
	return l4x_strnlen_user(src, ~0l);
}
EXPORT_SYMBOL(l4x_strlen_user);

#ifdef CONFIG_X86_64
long __copy_user_nocache(void *dst, const void __user *src,
                         unsigned size, int zerorest)
{
	/* As of 4.7, this is a x86-64 only function and only used in the
	 * verbs code. I do not really know what zerorest should mean, may
	 * be clear up the rest of the page, but there's no description in
	 * the asm function besides the asm itself. And as the only user of
	 * __copy_user_nocache uses it with zerorest == 0, we put the WARN
	 * for now: */
	WARN_ON(zerorest);
	return l4x_copy_from_user(dst, src, size);
}
EXPORT_SYMBOL(__copy_user_nocache);

long raw_copy_in_user(void __user *dst, const void __user *src,
                      unsigned long size)
{
	unsigned long src_page, src_offs = 0;
	unsigned long dst_page, dst_offs = 0;
	unsigned long _src = (unsigned long)src;
	unsigned long _dst = (unsigned long)dst;
	unsigned long flags;
	/* Disabling IRQs for the whole copy may be give bad preemptability,
	 * however we would need to re-walk page tables after having IRQs
	 * enabled otherwise. */
	local_irq_save(flags);
	while (size) {
		/* dummy for old irq flags, we already disabled them above */
		unsigned long flags_x;
		unsigned long copy_len;

		if (!src_offs) {
			src_page = parse_ptabs_read(_src, &src_offs, &flags_x);
			if (unlikely(src_page == -EFAULT)) {
				local_irq_restore(flags);
				return -EFAULT;
			}
		}

		if (!dst_offs) {
			dst_page = parse_ptabs_write(_dst, &dst_offs, &flags_x);
			if (unlikely(dst_page == -EFAULT)) {
				local_irq_restore(flags);
				return -EFAULT;
			}
		}

		copy_len = min((unsigned long)size,
		               PAGE_SIZE - max(src_offs, dst_offs));
		memcpy((void *)(dst_page + dst_offs),
		       (void *)(src_page + src_offs), copy_len);

		size -= copy_len;
		src_offs = (src_offs + copy_len) & (PAGE_SIZE - 1);
		dst_offs = (dst_offs + copy_len) & (PAGE_SIZE - 1);
		_src += copy_len;
		_dst += copy_len;
	}

	local_irq_restore(flags);
	return 0;
}
EXPORT_SYMBOL(raw_copy_in_user);
#endif /* X86_64 */
