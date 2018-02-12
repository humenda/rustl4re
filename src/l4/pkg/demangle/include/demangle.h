#ifndef L4_DEMANGLE
#define L4_DEMANGLE

#include <l4/sys/compiler.h>

/* Options passed to cplus_demangle (in 2nd parameter). */

#define DMGL_NO_OPTS	 0		/* For readability... */
#define DMGL_PARAMS	 (1 << 0)	/* Include function args */
#define DMGL_ANSI	 (1 << 1)	/* Include const, volatile, etc */
#define DMGL_JAVA	 (1 << 2)	/* Demangle as Java rather than C++. */
#define DMGL_VERBOSE	 (1 << 3)	/* Include implementation details.  */
#define DMGL_TYPES	 (1 << 4)	/* Also try to demangle type encodings.  */

#define DMGL_AUTO	 (1 << 8)
#define DMGL_GNU	 (1 << 9)
#define DMGL_LUCID	 (1 << 10)
#define DMGL_ARM	 (1 << 11)
#define DMGL_HP 	 (1 << 12)       /* For the HP aCC compiler;
                                            same as ARM except for
                                            template arguments, etc. */
#define DMGL_EDG	 (1 << 13)
#define DMGL_GNU_V3	 (1 << 14)
#define DMGL_GNAT	 (1 << 15)

EXTERN_C_BEGIN

extern char *cplus_demangle (const char *mangled, int options);

EXTERN_C_END

#endif
