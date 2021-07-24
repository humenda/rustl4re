#ifndef __INCLUDE__ASM__ARCH_X86__VSYSCALL_H__
#define __INCLUDE__ASM__ARCH_X86__VSYSCALL_H__

#include <asm-x86/vsyscall.h>

#ifdef CONFIG_X86_32
#undef VSYSCALL_ADDR
#define VSYSCALL_ADDR 0x0000000020000000
#endif

#endif /* ! __INCLUDE__ASM__ARCH_X86__VSYSCALL_H__ */
