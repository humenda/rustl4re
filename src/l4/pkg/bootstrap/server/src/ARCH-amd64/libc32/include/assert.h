#ifndef _ASSERT_H
#define _ASSERT_H

#include <cdefs.h>

__BEGIN_DECLS
/* This prints an "Assertion failed" message and aborts.  */
void __assert_fail (const char *__assertion, const char *__file,
		    unsigned int __line)
     __attribute__ ((__noreturn__));

__END_DECLS

#if (__GNUC__>=3)
#  define ASSERT_EXPECT_FALSE(exp)	__builtin_expect((exp), 0)
#else
#  define ASSERT_EXPECT_FALSE(exp)	(exp)
#endif

/* We don't show information about the current function since needs
 * additional space -- especially with gcc-2.95. The function name
 * can be found by searching the EIP in the kernel image. */
#undef assert
#ifdef NDEBUG
#define assert(expr)
#define check(expr) (void)(expr)
#else
# define assert(expr)						\
  ((void) ((ASSERT_EXPECT_FALSE(!(expr)))			\
	? (__assert_fail (#expr, __FILE__, __LINE__), 0)	\
	: 0))
# define check(expr) assert(expr)
#endif

#endif
