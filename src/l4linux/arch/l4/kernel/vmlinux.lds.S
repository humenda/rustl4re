/*
 * The location of this file is hard coded in the top-level Makefiles
 */
#ifdef CONFIG_X86
#include "arch-x86/vmlinux.lds.S"
#endif
#ifdef CONFIG_ARM
#include "arch-arm/vmlinux.lds.S"
#endif
