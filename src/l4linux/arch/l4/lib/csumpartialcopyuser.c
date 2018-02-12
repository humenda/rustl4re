
#include <net/checksum.h>
#include <asm/segment.h>
#include <asm/generic/memory.h>

static inline
unsigned int add_with_carry(unsigned x, unsigned y)
{
#ifdef CONFIG_ARM
	asm("adds  %0, %1, %0  \n"
	    "addcs %0, %0, #1  \n"
	    : "=r" (y) : "r" (x), "0" (y) : "cc");
#elif defined(CONFIG_X86)
	asm("addl %1, %0\n"
	    "adcl $0, %0\n"
	    : "=r" (y) : "g" (x), "0" (y) : "cc");
#else
#error Missing architecture implementation.
#endif
	return y;
}

static inline
void constant_memcpy(void *dst, const void *src, size_t len)
{
	switch (len) {
		case 1: __builtin_memcpy(dst, src, len); break; //*(uint8_t  *)(char *)dst = *(uint8_t  *)page; break;
		case 2: __builtin_memcpy(dst, src, len); break; //*(uint16_t *)(char *)dst = *(uint16_t *)page; break;
		case 3: __builtin_memcpy(dst, src, len); break;
		case 4: __builtin_memcpy(dst, src, len); break; //*(uint32_t *)(char *)dst = *(uint32_t *)page; break;
		default: BUG();
	};
}

__wsum csum_partial_copy_from_user(const void *src, void *dst,
                                   int len, __wsum sum, int *err_ptr)
{
	unsigned long page, offset;
	unsigned long flags;

	if (segment_eq(get_fs(), KERNEL_DS))
		return csum_partial_copy_nocheck(src, dst, len, sum);

	page = parse_ptabs_read((unsigned long)src, &offset, &flags);
	if (page == -EFAULT)
		goto exit_err;

	while (len) {
		unsigned long flags2;
		unsigned long len1, len2;

		len1 = min((unsigned long)len, PAGE_SIZE - ((unsigned long)src & ~PAGE_MASK));
		len2 = len - len1;

		if (len2 && (len1 & 3)) {
			u32 chksumgap = 0;
			unsigned d = len1 & 3;
			unsigned np_size = min(4UL - d, len - len1);

			if (len1 > d)
				sum = csum_partial_copy_nocheck((char *)page + offset, dst, len1 - d, sum);

			constant_memcpy(&chksumgap, (char *)page + offset + len1 - d, d);


			src += len1 + np_size;
			dst += len1 + np_size;


			page = parse_ptabs_read((unsigned long)src, &offset, &flags2);
			if (page == -EFAULT) {
				local_irq_restore(flags);
				goto exit_err;
			}

			constant_memcpy((char *)&chksumgap + d, (void *)page, np_size);

			if (1) {
				constant_memcpy((char *)dst - d - np_size, &chksumgap,
				                d + np_size);
				sum = add_with_carry(sum, chksumgap);
			} else {
				sum = csum_partial_copy_nocheck((char *)&chksumgap,
				                                dst - d - np_size,
				                                d + np_size, sum);
			}

			len -= len1 + np_size;
		} else {
			sum = csum_partial_copy_nocheck((char *)(page + offset), dst, len1, sum);

			src += len1;
			dst += len1;
			len -= len1;

			if (len) {
				page = parse_ptabs_read((unsigned long)src, &offset, &flags2);
				if (page == -EFAULT) {
					local_irq_restore(flags);
					goto exit_err;
				}
			}
		}
	}
	local_irq_restore(flags);
	return sum;

exit_err:
	*err_ptr = -EFAULT;
	return 0;
}
