#ifndef _ASM_X86_BUG_H
#define _ASM_X86_BUG_H

#ifdef CONFIG_BUG
#define HAVE_ARCH_BUG
#ifdef DDE_LINUX
#include <l4/dde/ddekit/panic.h>
#define BUG() ddekit_panic("\033[31mBUG():\033[0m %s:%d", __FILE__, __LINE__);

#define __WARN() ddekit_printf("\033[33mWARN():\033[0m%s:%d", __FILE__, __LINE__);
#else /* ! DDE_LINUX */

#ifdef CONFIG_DEBUG_BUGVERBOSE

#ifdef CONFIG_X86_32
# define __BUG_C0	"2:\t.long 1b, %c0\n"
#else
# define __BUG_C0	"2:\t.long 1b - 2b, %c0 - 2b\n"
#endif

#define BUG()							\
do {								\
	asm volatile("1:\tud2\n"				\
		     ".pushsection __bug_table,\"a\"\n"		\
		     __BUG_C0					\
		     "\t.word %c1, 0\n"				\
		     "\t.org 2b+%c2\n"				\
		     ".popsection"				\
		     : : "i" (__FILE__), "i" (__LINE__),	\
		     "i" (sizeof(struct bug_entry)));		\
	for (;;) ;						\
} while (0)

#else
#define BUG()							\
do {								\
	asm volatile("ud2");					\
	for (;;) ;						\
} while (0)
#endif

#endif /* DDE_LINUX */
#endif /* !CONFIG_BUG */

#include <asm-generic/bug.h>
#endif /* _ASM_X86_BUG_H */
