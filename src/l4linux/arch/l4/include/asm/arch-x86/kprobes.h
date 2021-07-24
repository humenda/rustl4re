#ifndef __ASM_L4__ARCH_I386__KPROBES_H__
#define __ASM_L4__ARCH_I386__KPROBES_H__

#include <asm-x86/kprobes.h>

/* We use 'hlt' as the break point instruction */

#undef BREAKPOINT_INSTRUCTION
#define BREAKPOINT_INSTRUCTION 0xf4

#endif /* ! __ASM_L4__ARCH_I386__KPROBES_H__ */
