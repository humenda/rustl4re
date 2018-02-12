/*
 * Copyright 2006 Andi Kleen, SUSE Labs.
 * Subject to the GNU Public License, v.2
 *
 * Fast user context implementation of getcpu()
 */

#include <linux/kernel.h>
#include <linux/getcpu.h>
#include <linux/time.h>
#include <asm/vgtod.h>

#ifdef CONFIG_L4
#include <asm/unistd.h>
#endif

notrace long
__vdso_getcpu(unsigned *cpu, unsigned *node, struct getcpu_cache *unused)
{
	unsigned int p;

#ifdef CONFIG_L4
	asm("syscall"
	    : "=a" (p)
	    : "0" (__NR_getcpu), "D" (cpu), "S" (node)
	    : "cc", "r11", "cx", "memory");

	return p;
#endif

	p = __getcpu();

	if (cpu)
		*cpu = p & VGETCPU_CPU_MASK;
	if (node)
		*node = p >> 12;
	return 0;
}

long getcpu(unsigned *cpu, unsigned *node, struct getcpu_cache *tcache)
	__attribute__((weak, alias("__vdso_getcpu")));
