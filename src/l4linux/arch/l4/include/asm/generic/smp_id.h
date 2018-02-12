#ifndef __INCLUDE__ASM__GENERIC__SMP_ID_H__
#define __INCLUDE__ASM__GENERIC__SMP_ID_H__

#ifdef CONFIG_SMP
int l4x_cpu_cpu_get(void);
#else
static inline int l4x_cpu_cpu_get(void)
{
	return 0;
}
#endif

#endif /* ! __INCLUDE__ASM__GENERIC__SMP_ID_H__ */
