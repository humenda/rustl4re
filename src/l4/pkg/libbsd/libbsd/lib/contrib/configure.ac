# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([libbsd], m4_esyscmd([./get-version]), [libbsd@lists.freedesktop.org])
AC_CONFIG_SRCDIR([src/strlcpy.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign nostdinc subdir-objects no-dist-gzip dist-xz])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])],
                            [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])

LIBBSD_ABI_MAJOR=0
LIBBSD_ABI_MINOR=8
LIBBSD_ABI_PATCH=6

LIBBSD_ABI="$LIBBSD_ABI_MAJOR:$LIBBSD_ABI_MINOR:$LIBBSD_ABI_PATCH"
AC_SUBST([LIBBSD_ABI])

# Check and store if we got user supplied variables
user_CFLAGS=${CFLAGS-unset}

# Checks for operating system services and capabilities.
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Set default compiler variables
if test "$user_CFLAGS" = unset && test "$GCC" = yes; then
  CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter"
fi

# Checks for libraries.
AC_CHECK_LIB([testu01], [unif01_CreateExternGenBits],
             [TESTU01_LIBS="-ltestu01"])
AC_SUBST([TESTU01_LIBS])
AM_CONDITIONAL([HAVE_LIBTESTU01],
               [test "x$ac_cv_lib_testu01_unif01_CreateExternGenBits" = "xyes"])

AS_CASE([$host_os],
  [*-gnu*], [
    # In old glibc versions (< 2.17) clock_gettime() is in librt.
    saved_LIBS="$LIBS"
    AC_SEARCH_LIBS([clock_gettime], [rt], [CLOCK_GETTIME_LIBS="-lrt"])
    AC_SUBST([CLOCK_GETTIME_LIBS])
    LIBS="$saved_LIBS"
  ],
  [*-musl*], [
    # Upstream refuses to define this, we will do it ourselves then.
    AC_DEFINE([__MUSL__], [1], [Define to 1 if we are building for musl])
  ],
)

# Checks for header files.
AC_CHECK_HEADERS([sys/ndir.h sys/dir.h ndir.h dirent.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_TYPEOF
AC_TYPE_INT64_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_CHECK_DECL([F_CLOSEM],
              [AC_DEFINE([HAVE_FCNTL_CLOSEM], [1],
                         [Define to 1 if you have fcntl(F_CLOSEM)])],
              [],
              [#include <limits.h>
               #include <fcntl.h>])

AC_CACHE_CHECK(
	[for GNU .init_array section support],
	[libbsd_cv_gnu_init_array_support],
	[AC_RUN_IFELSE(
		[AC_LANG_SOURCE(
[[
static int rc = 1;
static void init(int argc) { if (argc == 1) rc = 0; }
void (*init_func)(int argc) __attribute__((section(".init_array"))) = init;
int main() { return rc; }
]]
		)],
		[libbsd_cv_gnu_init_array_support=yes],
		[libbsd_cv_gnu_init_array_support=no],
		[AC_PREPROC_IFELSE(
			[AC_LANG_SOURCE(
[[
/* Look for a known libc that supports .init_array with the GNU extension
 * to pass main() arguments to the init functions. */
#include <stdlib.h>
#if defined __GLIBC_PREREQ
#  if __GLIBC_PREREQ(2, 4)
/* glibc supports GNU .init_array since 2.4. */
#  else
#    error glibc does not support GNU .init_array
#  endif
#else
/*
 * Basic SysV ABI .init_array support, init functions do not get arguments:
 * - Bionic since its inception.
 * - uClibc since 0.9.29.
 */
#  error unknown whether libc supports GNU .init_array
#endif
]]
			)],
			[libbsd_cv_gnu_init_array_support=yes],
			[libbsd_cv_gnu_init_array_support=no])
		]
	)]
)
AM_CONDITIONAL([BUILD_LIBBSD_CTOR],
               [test "$libbsd_cv_gnu_init_array_support" = yes])

# Checks for library functions.
AC_MSG_CHECKING([for program_invocation_short_name])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[#include <errno.h>]],
	                 [[const char *p = program_invocation_short_name;]])],
	[AC_DEFINE([HAVE_PROGRAM_INVOCATION_SHORT_NAME], [1],
	           [Define to 1 if you have program_invocation_short_name])
	 AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])])

AC_MSG_CHECKING([for __progname])
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[extern char *__progname;]],
	                 [[printf("%s", __progname);]])],
	[AC_DEFINE([HAVE___PROGNAME], [1], [Define to 1 if you have __progname])
	 AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])])

AC_CHECK_FUNCS([clearenv dirfd fopencookie __fpurge \
                getauxval getentropy getexecname getline \
                pstat_getproc sysconf])
AM_CONDITIONAL([HAVE_GETENTROPY], [test "x$ac_cv_func_getentropy" = "xtrue"])

AC_CONFIG_FILES([
	Makefile
	include/Makefile
	man/Makefile
	src/Makefile
	src/libbsd.pc
	src/libbsd-ctor.pc
	src/libbsd-overlay.pc
	test/Makefile
])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
