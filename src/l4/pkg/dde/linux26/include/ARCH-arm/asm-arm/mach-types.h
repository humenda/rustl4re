/*
 * This was automagically generated from include/asm-foo/mach-types.list!
 * Do NOT edit
 */

#ifndef __ASM_ARM_MACH_TYPE_H
#define __ASM_ARM_MACH_TYPE_H

#ifndef __ASSEMBLY__
/* The type of machine we're running on */
extern unsigned int __machine_arch_type;
#endif

/* see arch/arm/kernel/arch.c for a description of these */
#define MACH_TYPE_REALVIEW_EB          827


#ifdef CONFIG_MACH_REALVIEW_EB
# ifdef machine_arch_type
#  undef machine_arch_type
#  define machine_arch_type     __machine_arch_type
# else
#  define machine_arch_type     MACH_TYPE_REALVIEW_EB
# endif
# define machine_is_realview_eb()       (machine_arch_type == MACH_TYPE_REALVIEW_EB)
#else
# define machine_is_realview_eb()       (0)
#endif

/*
 * These have not yet been registered
 */

#ifndef machine_arch_type
#define machine_arch_type       __machine_arch_type
#endif

#endif
