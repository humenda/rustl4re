#ifndef __ASM_L4__L4X_I386__SYSCALL_H__
#define __ASM_L4__L4X_I386__SYSCALL_H__

enum {
	L4X_SYSCALL_NR_BASE = 0x30,
};

/*
 * Return syscall nr, or -1 if process is not on a syscall.
 */
static inline int l4x_l4syscall_get_nr(unsigned long error_code,
                                       unsigned long ip)
{
	int syscall_nr = 0;

	/* int 0xX is trap 0xd and err 0xX << 3 | 2 */
	if (error_code & 2)
		syscall_nr = error_code >> 3;

	if (syscall_nr < L4X_SYSCALL_NR_BASE
	    || syscall_nr >= (L4X_SYSCALL_NR_BASE + l4x_fiasco_nr_of_syscalls))
		return -1;

	return syscall_nr - L4X_SYSCALL_NR_BASE;
}

#endif /* ! __ASM_L4__L4X_I386__SYSCALL_H__ */
