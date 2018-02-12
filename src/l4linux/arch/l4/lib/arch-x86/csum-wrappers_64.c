
#include <net/checksum.h>
#include <asm/segment.h>
#include <asm/generic/memory.h>


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
				   int len, __wsum isum, int *errp)
{
	unsigned long page, offset;
	int len1, len2;
	unsigned long flags, flags2;

	/*
	 * Why 6, not 7? To handle odd addresses aligned we
	 * would need to do considerable complications to fix the
	 * checksum which is defined as an 16bit accumulator. The
	 * fix alignment code is primarily for performance
	 * compatibility with 32bit and that will handle odd
	 * addresses slowly too.
	 */
	if (unlikely((unsigned long)src & 6)) {
		while (((unsigned long)src & 6) && len >= 2) {
			__u16 val16;

			*errp = __get_user(val16, (const __u16 __user *)src);
			if (*errp)
				return isum;

			*(__u16 *)dst = val16;
			isum = (__force __wsum)add32_with_carry(
					(__force unsigned)isum, val16);
			src += 2;
			dst += 2;
			len -= 2;
		}
	}

	*errp = 0;
	if (segment_eq(get_fs(), KERNEL_DS))
		return csum_partial_copy_generic(src, dst, len, isum, NULL, NULL);

	page = parse_ptabs_read((unsigned long)src, &offset, &flags);
	if (page == -EFAULT)
		goto exit_err;

	while (len) {
		len1 = min(len, (int)PAGE_SIZE - (int)((unsigned long) src & ~PAGE_MASK));
		len2 = len - len1;

		if (len2 && (len1 & 3)) {
			u32 chksumgap = 0;
			int d = len1 & 3;
			int np_size = min(4 - d, len - len1);

			if (len1 > d)
				isum = csum_partial_copy_generic((u8*)(page + offset), dst, len1 - d, isum, NULL, NULL);
			constant_memcpy(&chksumgap, (char *)page + offset + len1 - d, d);

			src += len1 + np_size;
			dst += len1 + np_size;

			page = parse_ptabs_read((unsigned long)src, &offset, &flags2);
			if (page == -EFAULT)
				goto exit_err;

			constant_memcpy((char *)&chksumgap + d, (void *)page, np_size);
			constant_memcpy((char *)dst - d - np_size, &chksumgap,
			                d + np_size);
			isum = (__force __wsum)add32_with_carry((__force unsigned)isum, chksumgap);
			len -= len1 + np_size;
		} else  {
			isum = csum_partial_copy_generic((u8*)(page + offset), dst, len1, isum, NULL, NULL);

			src += len1;
			dst += len1;
			len -= len1;
			if (len) {
				page = parse_ptabs_read((unsigned long)src, &offset, &flags2);
				if (page == -EFAULT)
					goto exit_err;
			}
		}
	}

	local_irq_restore(flags);
	return isum;

 exit_err:
	local_irq_restore(flags);
	*errp = -EFAULT;
	return 0;
}

/**
 * csum_partial_copy_to_user - Copy and checksum to user space.
 * @src: source address
 * @dst: destination address (user space)
 * @len: number of bytes to be copied.
 * @isum: initial sum that is added into the result (32bit unfolded)
 * @errp: set to -EFAULT for an bad destination address.
 *
 * Returns an 32bit unfolded checksum of the buffer.
 * src and dst are best aligned to 64bits.
 */
__wsum
csum_partial_copy_to_user(const void *src, void __user *dst,
			  int len, __wsum isum, int *errp)
{
	unsigned long page, offset;
	int len1, len2;
	unsigned long flags, flags2;

	might_sleep();

	if (unlikely(!access_ok(VERIFY_WRITE, dst, len))) {
		*errp = -EFAULT;
		return 0;
	}

	if (unlikely((unsigned long)dst & 6)) {
		while (((unsigned long)dst & 6) && len >= 2) {
			__u16 val16 = *(__u16 *)src;

			isum = (__force __wsum)add32_with_carry(
					(__force unsigned)isum, val16);
			*errp = __put_user(val16, (__u16 __user *)dst);
			if (*errp)
				return isum;
			src += 2;
			dst += 2;
			len -= 2;
		}
	}

	*errp = 0;
	if (segment_eq(get_fs(), KERNEL_DS))
		return csum_partial_copy_generic(src, dst, len, isum, NULL, NULL);

	page = parse_ptabs_write((unsigned long)dst, &offset, &flags);
	if (page == -EFAULT)
		goto exit_err;

	while (len) {
		len1 = min(len, (int)PAGE_SIZE - (int)((unsigned long) dst & ~PAGE_MASK));
		len2 = len - len1;

		if (len2 && (len1 & 3)) {
			u32 chksumgap = 0;
			int d = len1 & 3;
			int np_size = min(4 - d, len - len1);

			if (len1 > d)
				isum = csum_partial_copy_generic(src, (char *)(page + offset), len1 - d, isum, NULL, NULL);
			chksumgap = *(u32*)(((u8*)src) + len1 - d);
			constant_memcpy((char *)page + offset + len1 - d, &chksumgap, d);

			src += len1 + np_size;
			dst += len1 + np_size;

			page = parse_ptabs_write((unsigned long)dst, &offset, &flags2);
			if (page == -EFAULT)
				goto exit_err;

			constant_memcpy((void *)page, (u8 *)&chksumgap + d, np_size);

			isum = (__force __wsum)add32_with_carry((__force unsigned)isum, chksumgap);
			len -= len1 + np_size;
		} else  {
			isum = csum_partial_copy_generic(src, (char *)(page + offset), len1, isum, NULL, NULL);

			src += len1;
			dst += len1;
			len -= len1;
			if (len) {
				page = parse_ptabs_write((unsigned long)dst, &offset, &flags2);
				if (page == -EFAULT)
					goto exit_err;
			}
		}
	}

	local_irq_restore(flags);
	return isum;

 exit_err:
	local_irq_restore(flags);
	*errp = -EFAULT;
	return 0;
}
EXPORT_SYMBOL(csum_partial_copy_to_user);

/**
 * csum_partial_copy_nocheck - Copy and checksum.
 * @src: source address
 * @dst: destination address
 * @len: number of bytes to be copied.
 * @sum: initial sum that is added into the result (32bit unfolded)
 *
 * Returns an 32bit unfolded checksum of the buffer.
 */
__wsum
csum_partial_copy_nocheck(const void *src, void *dst, int len, __wsum sum)
{
	return csum_partial_copy_generic(src, dst, len, sum, NULL, NULL);
}
EXPORT_SYMBOL(csum_partial_copy_nocheck);

__sum16 csum_ipv6_magic(const struct in6_addr *saddr,
			const struct in6_addr *daddr,
			__u32 len, __u8 proto, __wsum sum)
{
	__u64 rest, sum64;

	rest = (__force __u64)htonl(len) + (__force __u64)htons(proto) +
		(__force __u64)sum;

	asm("	addq (%[saddr]),%[sum]\n"
	    "	adcq 8(%[saddr]),%[sum]\n"
	    "	adcq (%[daddr]),%[sum]\n"
	    "	adcq 8(%[daddr]),%[sum]\n"
	    "	adcq $0,%[sum]\n"

	    : [sum] "=r" (sum64)
	    : "[sum]" (rest), [saddr] "r" (saddr), [daddr] "r" (daddr));

	return csum_fold(
	       (__force __wsum)add32_with_carry(sum64 & 0xffffffff, sum64>>32));
}
EXPORT_SYMBOL(csum_ipv6_magic);
