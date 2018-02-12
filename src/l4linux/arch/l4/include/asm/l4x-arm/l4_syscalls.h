#ifndef __ASM_L4__L4X_ARM__SYSCALL_H__
#define __ASM_L4__L4X_ARM__SYSCALL_H__

/*
 * Return syscall nr, or -1 if process is not on a syscall.
 */
static inline int l4x_l4syscall_get_nr(unsigned long error_code,
                                       unsigned long ip)
{
	unsigned long val = ~ip;

	if (val < 8 || val > 40 || val % 4 != 3)
		return -1;

	return (val >> 2) - 2;
}

#endif /* ! __ASM_L4__L4X_ARM__SYSCALL_H__ */
