/* Separate file to not mix Linux and L4Re includes */

#ifdef CONFIG_X86
#include <asm/nospec-branch.h>
#endif

#include "launch.h"

int launch_kernel(int argc, char **argv,
                  void *exchgp, unsigned long entry)
{
	unsigned long ret;

#ifdef ARCH_arm
	register unsigned long _argc  asm("r0") = argc;
	register unsigned long _argv  asm("r1") = (unsigned long)argv;
	register unsigned long _exchg asm("r2") = (unsigned long)exchgp;
	register unsigned long _entry asm("r3") = entry;
	asm volatile("mov lr, pc   \n"
	             "mov pc, r3   \n"
	             "mov %0, r0   \n"
	             : "=r" (ret)
	             : "r" (_argv),
	               "r" (_argc),
	               "r" (_exchg),
	               "r" (_entry)
	             : "memory");
#elif defined(ARCH_x86)
	asm volatile("push %[argv]\n"
	             "push %[argc]\n"
	             ANNOTATE_RETPOLINE_SAFE
	             "mov  %[exchg], %%esi\n"
	             "call  *%[entry]\n"
	             "pop %[argc]\n"
	             "pop %[argv]\n"
	            : "=a" (ret)
	            : [argv] "r" (argv),
	              [argc] "r" (argc),
	              [exchg] "r" (exchgp),
	              [entry] "r" (entry)
	            : "memory", "esi");
#else
	asm volatile("movq  %[exchg], %%rcx\n"
	             ANNOTATE_RETPOLINE_SAFE
	             "call  *%[entry]\n"
	             : "=a" (ret)
	             : [argv] "S" (argv),
	               [argc] "D" ((unsigned long)argc),
	               [exchg] "r" (exchgp),
	               [entry] "r" (entry)
	             : "memory", "rcx");
#endif

	return ret;
}
