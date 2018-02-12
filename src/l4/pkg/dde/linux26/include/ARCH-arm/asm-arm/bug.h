#ifndef _ASMARM_BUG_H
#define _ASMARM_BUG_H

#ifdef CONFIG_BUG

#ifdef DDE_LINUX
#include <l4/dde/ddekit/panic.h>
#define BUG() ddekit_panic("\033[31mBUG():\033[0m %s:%d", __FILE__, __LINE__);

#define __WARN() ddekit_printf("\033[33mWARN():\033[0m%s:%d", __FILE__, __LINE__);
#else /* ! DDE_LINUX */

#ifdef CONFIG_DEBUG_BUGVERBOSE
extern void __bug(const char *file, int line) __attribute__((noreturn));

/* give file/line information */
#define BUG()		__bug(__FILE__, __LINE__)

#else

/* this just causes an oops */
#define BUG()		do { *(int *)0 = 0; } while (1)

#endif

#endif /* DDE_LINUX */

#define HAVE_ARCH_BUG
#endif

#include <asm-generic/bug.h>

#endif
