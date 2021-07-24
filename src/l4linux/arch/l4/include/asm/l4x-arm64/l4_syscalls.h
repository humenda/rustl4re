#ifndef __ASM_L4__L4X_ARM64__L4_SYSCALL_H__
#define __ASM_L4__L4X_ARM64__L4_SYSCALL_H__

/*
 * Return syscall nr, or -1 if process is not on a syscall.
 */
static inline int l4x_l4syscall_get_nr(unsigned long error_code,
                                       unsigned long ip)
{
	return -1; // TODO?
}

#endif /* ! __ASM_L4__L4X_ARM64__L4_SYSCALL_H__ */
